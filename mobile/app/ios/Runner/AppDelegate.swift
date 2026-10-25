import Flutter
import UIKit
import os

@main
@objc class AppDelegate: FlutterAppDelegate {
  private var backgroundChannel: FlutterMethodChannel?
  private var memoryChannel: FlutterMethodChannel?
  private var backgroundTasks: [Int: UIBackgroundTaskIdentifier] = [:]
  private var nextTaskID = 1

  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    GeneratedPluginRegistrant.register(with: self)
    let launched = super.application(application, didFinishLaunchingWithOptions: launchOptions)
    installNativeChannels()
    return launched
  }

  override func applicationDidBecomeActive(_ application: UIApplication) {
    super.applicationDidBecomeActive(application)
    // A host may attach its Flutter controller after didFinishLaunching.
    installNativeChannels()
  }

  override func applicationWillTerminate(_ application: UIApplication) {
    disposeNativeChannels()
    super.applicationWillTerminate(application)
  }

  private func installNativeChannels() {
    guard backgroundChannel == nil else { return }
    // The registry supplies the engine's messenger without assuming the root
    // controller is a FlutterViewController. Retry on activation if not ready.
    guard let registrar = registrar(forPlugin: "Stl2StpNativeRuntime") else { return }
    let messenger = registrar.messenger()
    let background = FlutterMethodChannel(name: "stl2stp/background", binaryMessenger: messenger)
    let memory = FlutterMethodChannel(name: "stl2stp/memory", binaryMessenger: messenger)
    backgroundChannel = background
    memoryChannel = memory

    background.setMethodCallHandler { [weak self] call, result in
      guard let self = self else {
        result(FlutterError(code: "unavailable", message: "Native runtime is unavailable", details: nil))
        return
      }
      switch call.method {
      case "begin":
        let arguments = call.arguments as? [String: Any]
        let name = arguments?["name"] as? String ?? "stl2stp-convert"
        result(self.beginBackgroundTask(name: name))
      case "end":
        guard let arguments = call.arguments as? [String: Any], let id = arguments["id"] as? Int else {
          result(FlutterError(code: "invalid_arguments", message: "end requires a task id", details: nil))
          return
        }
        self.endBackgroundTask(id: id)
        result(nil)
      default:
        result(FlutterMethodNotImplemented)
      }
    }

    memory.setMethodCallHandler { call, result in
      switch call.method {
      case "available":
        result(Int(os_proc_available_memory()))
      default:
        result(FlutterMethodNotImplemented)
      }
    }
  }

  private func beginBackgroundTask(name: String) -> Int? {
    let id = nextTaskID
    nextTaskID += 1
    let task = UIApplication.shared.beginBackgroundTask(withName: name) { [weak self] in
      // UIKit invokes expiration handlers on the main thread. Queue the work
      // so registration also finishes if expiration is reported immediately.
      DispatchQueue.main.async { [weak self] in
        guard let self = self, self.backgroundTasks[id] != nil else { return }
        self.backgroundChannel?.invokeMethod("expired", arguments: id)
        self.endBackgroundTask(id: id)
      }
    }
    guard task != .invalid else { return nil }
    backgroundTasks[id] = task
    return id
  }

  private func endBackgroundTask(id: Int) {
    guard let task = backgroundTasks.removeValue(forKey: id) else { return }
    UIApplication.shared.endBackgroundTask(task)
  }

  private func disposeNativeChannels() {
    backgroundChannel?.setMethodCallHandler(nil)
    memoryChannel?.setMethodCallHandler(nil)
    backgroundChannel = nil
    memoryChannel = nil
    let tasks = Array(backgroundTasks.values)
    backgroundTasks.removeAll()
    for task in tasks {
      UIApplication.shared.endBackgroundTask(task)
    }
  }
}
