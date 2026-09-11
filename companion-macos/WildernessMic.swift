import AppKit
import ApplicationServices
import AudioToolbox
import AVFoundation
import CoreBluetooth
import Foundation

private let serviceUUID = CBUUID(string: "0000A2B0-0000-1000-8000-00805F9B34FB")
private let controlUUID = CBUUID(string: "A2B1")
private let inputUUID = CBUUID(string: "A2B2")
private let audioUUID = CBUUID(string: "A2B3")
private let blockBytes = 804
private let blockSamples = 1600

private let steps: [Int] = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
    4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
    10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
    24623, 27086, 29794, 32767,
]
private let indexDelta = [-1, -1, -1, -1, 2, 4, 6, 8,
                          -1, -1, -1, -1, 2, 4, 6, 8]

private func decode(_ block: Data) -> Data? {
    guard block.count == blockBytes else { return nil }
    let bytes = [UInt8](block)
    var predictor = Int(Int16(bitPattern: UInt16(bytes[0]) | UInt16(bytes[1]) << 8))
    var index = min(88, Int(bytes[2]))
    var samples = [Int16]()
    samples.reserveCapacity(blockSamples)
    samples.append(Int16(predictor))
    var readIndex = 4
    for sampleIndex in 1..<blockSamples {
        let code: Int
        if sampleIndex & 1 == 1 {
            code = Int(bytes[readIndex] & 0x0f)
        } else {
            code = Int((bytes[readIndex] >> 4) & 0x0f)
            readIndex += 1
        }
        let step = steps[index]
        var difference = step >> 3
        if code & 4 != 0 { difference += step }
        if code & 2 != 0 { difference += step >> 1 }
        if code & 1 != 0 { difference += step >> 2 }
        predictor += (code & 8 != 0) ? -difference : difference
        predictor = min(32767, max(-32768, predictor))
        index = min(88, max(0, index + indexDelta[code]))
        samples.append(Int16(predictor))
    }
    return samples.withUnsafeBytes { Data($0) }
}

private extension Data {
    mutating func appendLE<T: FixedWidthInteger>(_ value: T) {
        var little = value.littleEndian
        Swift.withUnsafeBytes(of: &little) { append(contentsOf: $0) }
    }
}

private func wavData(pcm: Data) -> Data {
    var output = Data("RIFF".utf8)
    output.appendLE(UInt32(36 + pcm.count))
    output.append(Data("WAVEfmt ".utf8))
    output.appendLE(UInt32(16))
    output.appendLE(UInt16(1))
    output.appendLE(UInt16(1))
    output.appendLE(UInt32(16_000))
    output.appendLE(UInt32(32_000))
    output.appendLE(UInt16(2))
    output.appendLE(UInt16(16))
    output.append(Data("data".utf8))
    output.appendLE(UInt32(pcm.count))
    output.append(pcm)
    return output
}

final class VirtualMicOutput {
    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private let sourceFormat = AVAudioFormat(standardFormatWithSampleRate: 16_000,
                                             channels: 1)!
    private(set) var isActive = false

    init() {
        guard let device = Self.findOutputDevice(containing: "BlackHole 2ch") else {
            print("未检测到 BlackHole 2ch；当前只保存 WAV，不会送入豆包。")
            return
        }
        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: sourceFormat)
        guard let audioUnit = engine.outputNode.audioUnit else {
            print("BlackHole 初始化失败：无法取得输出 AudioUnit。")
            return
        }
        var selectedDevice = device
        let status = AudioUnitSetProperty(audioUnit,
                                          kAudioOutputUnitProperty_CurrentDevice,
                                          kAudioUnitScope_Global, 0,
                                          &selectedDevice,
                                          UInt32(MemoryLayout<AudioDeviceID>.size))
        guard status == noErr else {
            print("BlackHole 初始化失败：无法选择设备（OSStatus \(status)）。")
            return
        }
        do {
            try engine.start()
            player.play()
            isActive = true
            print("实时音频输出已连接到 BlackHole 2ch。")
        } catch {
            print("BlackHole 音频引擎启动失败：\(error.localizedDescription)")
        }
    }

    func reset() {
        guard isActive else { return }
        player.stop()
        player.play()
    }

    func enqueue(pcm: Data) {
        guard isActive, !pcm.isEmpty else { return }
        let frames = AVAudioFrameCount(pcm.count / MemoryLayout<Int16>.size)
        guard let buffer = AVAudioPCMBuffer(pcmFormat: sourceFormat,
                                            frameCapacity: frames),
              let destination = buffer.floatChannelData?[0] else { return }
        buffer.frameLength = frames
        pcm.withUnsafeBytes { raw in
            let source = raw.bindMemory(to: Int16.self)
            for index in 0..<Int(frames) {
                destination[index] = Float(source[index]) / 32768.0
            }
        }
        player.scheduleBuffer(buffer)
    }

    private static func findOutputDevice(containing target: String) -> AudioDeviceID? {
        var address = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDevices,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var size: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(AudioObjectID(kAudioObjectSystemObject),
                                             &address, 0, nil, &size) == noErr else { return nil }
        var devices = [AudioDeviceID](repeating: 0,
                                      count: Int(size) / MemoryLayout<AudioDeviceID>.size)
        guard AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject),
                                         &address, 0, nil, &size, &devices) == noErr else { return nil }
        for device in devices {
            var nameAddress = AudioObjectPropertyAddress(
                mSelector: kAudioObjectPropertyName,
                mScope: kAudioObjectPropertyScopeGlobal,
                mElement: kAudioObjectPropertyElementMain)
            var unmanagedName: Unmanaged<CFString>?
            var nameSize = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
            if AudioObjectGetPropertyData(device, &nameAddress, 0, nil,
                                          &nameSize, &unmanagedName) == noErr,
               let name = unmanagedName?.takeUnretainedValue(),
               (name as String).localizedCaseInsensitiveContains(target) {
                return device
            }
        }
        return nil
    }
}

final class WildernessMic: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let audioQueue = DispatchQueue(label: "cc.wmedia.wilderness-mic.audio",
                                           qos: .userInitiated)
    private let virtualMic = VirtualMicOutput()
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var control: CBCharacteristic?
    private var input: CBCharacteristic?
    private var audio: CBCharacteristic?
    private var leftControlDown = false
    private var requested = false
    private var currentSequence: UInt8?
    private var lastFragment = -1
    private var compressed = Data()
    private var pcm = Data()
    private var receivedBlocks = 0
    private var droppedBlocks = 0
    private var callbackSizes: [Int: Int] = [:]
    private let outputDirectory: URL
    private var monitor: Any?
    private var commandLoopStarted = false
    private var ignoreControlMonitorUntil = Date.distantPast

    init(outputDirectory: URL) {
        self.outputDirectory = outputDirectory
        super.init()
        try? FileManager.default.createDirectory(at: outputDirectory,
                                                 withIntermediateDirectories: true)
        central = CBCentralManager(delegate: self, queue: .main)
        monitor = NSEvent.addGlobalMonitorForEvents(matching: .flagsChanged) { [weak self] event in
            self?.modifierChanged(event)
        }
        startCommandLoop()
        let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true]
            as CFDictionary
        if !AXIsProcessTrustedWithOptions(options) {
            print("需要在系统设置的辅助功能权限中允许当前终端，工牌才能代按 Control 和回车。")
        }
        print("已启动：工牌语音界面可控制豆包；实体左 Control 仍可用于诊断。")
        print("首次运行若 macOS 提示，请允许当前终端/应用读取键盘输入。")
        print("诊断命令：start / stop，或 record 3 自动录制三秒。")
    }

    deinit {
        if let monitor { NSEvent.removeMonitor(monitor) }
    }

    private func modifierChanged(_ event: NSEvent) {
        guard event.keyCode == 59 else { return }
        let down = event.modifierFlags.contains(.control)
        guard down != leftControlDown else { return }
        leftControlDown = down
        if Date() < ignoreControlMonitorUntil { return }
        requestStreaming(down)
    }

    private func postKey(_ keyCode: CGKeyCode, down: Bool,
                         flags: CGEventFlags = []) {
        guard let source = CGEventSource(stateID: .hidSystemState),
              let event = CGEvent(keyboardEventSource: source,
                                  virtualKey: keyCode,
                                  keyDown: down) else {
            print("无法创建键盘事件。")
            return
        }
        event.flags = flags
        event.post(tap: .cghidEventTap)
    }

    private func postControl(_ down: Bool) {
        ignoreControlMonitorUntil = Date().addingTimeInterval(1.0)
        leftControlDown = down
        postKey(59, down: down, flags: down ? [.maskControl] : [])
    }

    private func postReturn() {
        postKey(36, down: true)
        postKey(36, down: false)
    }

    private func beginLocalRecording() {
        audioQueue.async { [weak self] in self?.resetRecording() }
        print("开始从 AI Passport 收音…")
    }

    private func finishLocalRecording() {
        audioQueue.asyncAfter(deadline: .now() + .milliseconds(250)) { [weak self] in
            self?.saveRecording()
        }
    }

    private func handleBadgeInput(_ value: UInt8) {
        switch value {
        case 1:
            beginLocalRecording()
            postControl(true)
            print("工牌 UP 按下：已代按左 Control。")
        case 2:
            postControl(false)
            finishLocalRecording()
            print("工牌 UP 松开：已释放左 Control。")
        case 3:
            postReturn()
            print("工牌 DOWN：已发送回车。")
        default:
            print("忽略未知工牌输入事件：\(value)")
        }
    }

    private func startCommandLoop() {
        guard !commandLoopStarted else { return }
        commandLoopStarted = true
        DispatchQueue.global(qos: .utility).async { [weak self] in
            while let command = readLine()?.trimmingCharacters(in: .whitespacesAndNewlines) {
                DispatchQueue.main.async {
                    let parts = command.lowercased().split(separator: " ")
                    switch parts.first {
                    case "start": self?.requestStreaming(true)
                    case "stop": self?.requestStreaming(false)
                    case "record":
                        let seconds = parts.count == 2 ? Double(parts[1]) : nil
                        guard let seconds, (0.5...60).contains(seconds) else {
                            print("用法：record <0.5 到 60 秒>")
                            return
                        }
                        self?.requestStreaming(true)
                        DispatchQueue.main.asyncAfter(deadline: .now() + seconds) {
                            self?.requestStreaming(false)
                        }
                    default:
                        print("未知命令：\(command)。可用命令为 start / stop / record <秒>。")
                    }
                }
            }
        }
    }

    private func requestStreaming(_ start: Bool) {
        requestStreamingNow(start)
    }

    private func requestStreamingNow(_ start: Bool) {
        requested = start
        guard let peripheral, let control else {
            print(start ? "左 Control 已按下，但工牌还未连接。" : "左 Control 已松开。")
            return
        }
        if start {
            beginLocalRecording()
        }
        peripheral.writeValue(Data([start ? 1 : 0]), for: control, type: .withoutResponse)
        if !start {
            finishLocalRecording()
        }
    }

    private func resetRecording() {
        pcm.removeAll(keepingCapacity: true)
        receivedBlocks = 0
        droppedBlocks = 0
        callbackSizes.removeAll(keepingCapacity: true)
        currentSequence = nil
        compressed.removeAll(keepingCapacity: true)
        virtualMic.reset()
    }

    private func saveRecording() {
        guard !pcm.isEmpty else {
            print("本次没有收到音频。")
            return
        }
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        let url = outputDirectory.appendingPathComponent("ai-passport-\(formatter.string(from: Date())).wav")
        do {
            try wavData(pcm: pcm).write(to: url, options: .atomic)
            let seconds = Double(pcm.count) / 32_000.0
            print(String(format: "已保存 %.1f 秒：%@（完整块 %d，丢块 %d）",
                         seconds, url.path, receivedBlocks, droppedBlocks))
            let distribution = callbackSizes.keys.sorted().map {
                "\($0)B×\(callbackSizes[$0]!)"
            }.joined(separator: ", ")
            print("BLE 回调长度：\(distribution)")
        } catch {
            print("保存 WAV 失败：\(error.localizedDescription)")
        }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard central.state == .poweredOn else {
            print("蓝牙不可用：\(central.state.rawValue)")
            return
        }
        print("正在扫描 Wilderness Mic…")
        central.scanForPeripherals(withServices: [serviceUUID], options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false,
        ])
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        self.peripheral = peripheral
        central.stopScan()
        peripheral.delegate = self
        print("已发现 \(peripheral.name ?? "Wilderness Mic")，正在连接…")
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("蓝牙已连接，正在发现麦克风服务…")
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                        timestamp: CFAbsoluteTime, isReconnecting: Bool, error: Error?) {
        control = nil
        input = nil
        audio = nil
        print("工牌断开，重新扫描…")
        central.scanForPeripherals(withServices: [serviceUUID])
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil else { print("发现服务失败：\(error!)"); return }
        for service in peripheral.services ?? [] where service.uuid == serviceUUID {
            peripheral.discoverCharacteristics([controlUUID, inputUUID, audioUUID],
                                               for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard error == nil else { print("发现特征失败：\(error!)"); return }
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == controlUUID { control = characteristic }
            if characteristic.uuid == inputUUID {
                input = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            }
            if characteristic.uuid == audioUUID {
                audio = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
        if control != nil && input != nil && audio != nil {
            print("Wilderness Mic 已就绪。可从工牌长按 UP 进入语音模式。")
            if requested { requestStreamingNow(true) }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil, let packet = characteristic.value else { return }
        if characteristic.uuid == inputUUID, packet.count == 1 {
            handleBadgeInput(packet[packet.startIndex])
        } else if characteristic.uuid == audioUUID, packet.count >= 2 {
            audioQueue.async { [weak self] in self?.consume(packet) }
        }
    }

    private func consume(_ packet: Data) {
        callbackSizes[packet.count, default: 0] += 1
        let bytes = [UInt8](packet)
        let sequence = bytes[0]
        let fragment = Int(bytes[1] & 0x7f)
        let isLast = bytes[1] & 0x80 != 0
        if currentSequence != sequence {
            if currentSequence != nil && !compressed.isEmpty { droppedBlocks += 1 }
            currentSequence = sequence
            lastFragment = -1
            compressed.removeAll(keepingCapacity: true)
        }
        guard fragment == lastFragment + 1 else {
            if fragment > lastFragment { lastFragment = fragment }
            return
        }
        compressed.append(packet.dropFirst(2))
        lastFragment = fragment
        if isLast {
            if let decoded = decode(compressed) {
                pcm.append(decoded)
                virtualMic.enqueue(pcm: decoded)
                receivedBlocks += 1
            } else {
                droppedBlocks += 1
            }
            currentSequence = nil
            compressed.removeAll(keepingCapacity: true)
        }
    }
}

let applicationSupport = FileManager.default.urls(
    for: .applicationSupportDirectory,
    in: .userDomainMask
).first ?? URL(fileURLWithPath: NSTemporaryDirectory(), isDirectory: true)
let output = applicationSupport
    .appendingPathComponent("WildernessMic", isDirectory: true)
    .appendingPathComponent("recordings", isDirectory: true)
let app = WildernessMic(outputDirectory: output)
withExtendedLifetime(app) {
    RunLoop.main.run()
}
