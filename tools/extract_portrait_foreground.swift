import CoreGraphics
import CoreImage
import Foundation
import Vision

enum CutoutError: Error {
    case invalidArguments
    case unreadableImage
    case noForeground
    case missingColorSpace
}

guard CommandLine.arguments.count == 3 else {
    throw CutoutError.invalidArguments
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2])
guard let image = CIImage(
    contentsOf: inputURL,
    options: [.applyOrientationProperty: true]
) else {
    throw CutoutError.unreadableImage
}

let request = VNGenerateForegroundInstanceMaskRequest()
let handler = VNImageRequestHandler(ciImage: image)
try handler.perform([request])
guard let observation = request.results?.first,
      !observation.allInstances.isEmpty else {
    throw CutoutError.noForeground
}

let maskBuffer = try observation.generateScaledMaskForImage(
    forInstances: observation.allInstances,
    from: handler
)
let mask = CIImage(cvPixelBuffer: maskBuffer)
let transparent = CIImage(color: .clear).cropped(to: image.extent)
let cutout = image.applyingFilter(
    "CIBlendWithMask",
    parameters: [
        kCIInputBackgroundImageKey: transparent,
        kCIInputMaskImageKey: mask,
    ]
)

guard let colorSpace = CGColorSpace(name: CGColorSpace.sRGB) else {
    throw CutoutError.missingColorSpace
}
try CIContext().writePNGRepresentation(
    of: cutout,
    to: outputURL,
    format: .RGBA8,
    colorSpace: colorSpace
)
