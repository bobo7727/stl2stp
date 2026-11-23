import Flutter
import Foundation

/// Bridges `StoreCoordinator` to Dart: method channel `stl2stp/store` and event
/// channel `stl2stp/store/events`. Installed independently of the background
/// and memory channels, and idempotent.
final class StoreChannelHost: NSObject, FlutterStreamHandler {
  private var events: FlutterEventSink?

  func install(messenger: FlutterBinaryMessenger) {
    let method = FlutterMethodChannel(name: "stl2stp/store", binaryMessenger: messenger)
    let stream = FlutterEventChannel(name: "stl2stp/store/events", binaryMessenger: messenger)
    stream.setStreamHandler(self)
    method.setMethodCallHandler { call, result in
      let args = call.arguments as? [String: Any]
      Task { @MainActor in
        let c = StoreCoordinator.shared
        switch call.method {
        case "start": await c.begin(); result(["type": "started"])
        case "loadProducts": result(await c.loadProducts())
        case "buyTip": result(await c.buyTip(productId: args?["productId"] as? String ?? ""))
        default: result(FlutterMethodNotImplemented)
        }
      }
    }
    Task {
      await StoreCoordinator.shared.setSink { [weak self] event in
        DispatchQueue.main.async { self?.events?(event) }
      }
    }
  }

  func onListen(withArguments arguments: Any?, eventSink events: @escaping FlutterEventSink) -> FlutterError? {
    self.events = events
    return nil
  }

  func onCancel(withArguments arguments: Any?) -> FlutterError? {
    events = nil
    return nil
  }
}
