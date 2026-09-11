import Foundation
import ImageIO
import Vision

// Decode exactly one QR code from a local image and emit a C byte-array header.
// The payload is intentionally never printed because employee QR data is private
// provisioning material. The generated header should remain ignored by Git.
guard CommandLine.arguments.count == 3 else {
    fputs("Usage: extract_private_qr.swift INPUT_IMAGE OUTPUT_HEADER\n", stderr)
    exit(2)
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2])

guard let source = CGImageSourceCreateWithURL(inputURL as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
    fputs("Unable to read input image\n", stderr)
    exit(3)
}

let request = VNDetectBarcodesRequest()
request.symbologies = [.qr]
let handler = VNImageRequestHandler(cgImage: image, options: [:])

do {
    try handler.perform([request])
} catch {
    fputs("QR detection failed: \(error)\n", stderr)
    exit(4)
}

let observations = (request.results ?? []).filter { $0.payloadStringValue != nil }
guard observations.count == 1,
      let payload = observations[0].payloadStringValue else {
    fputs("Expected exactly one decodable QR code, found \(observations.count)\n", stderr)
    exit(5)
}

let bytes = Array(payload.utf8)
guard !bytes.isEmpty else {
    fputs("Decoded QR payload is empty\n", stderr)
    exit(6)
}

let rows = stride(from: 0, to: bytes.count, by: 12).map { start -> String in
    let end = min(start + 12, bytes.count)
    return "    " + bytes[start..<end].map { String(format: "0x%02X", $0) }.joined(separator: ", ")
}

let header = """
// Generated locally from the employee's QR image. Do not commit or publish.
#pragma once
#include <stdint.h>

static const uint8_t WECHAT_QR_DATA[] = {
\(rows.joined(separator: ",\n"))
};
static const uint32_t WECHAT_QR_DATA_LEN = \(bytes.count);

"""

do {
    try header.write(to: outputURL, atomically: true, encoding: .utf8)
} catch {
    fputs("Unable to write output header: \(error)\n", stderr)
    exit(7)
}

print("QR decode: PASS (one code, \(bytes.count) UTF-8 bytes; payload not printed)")
