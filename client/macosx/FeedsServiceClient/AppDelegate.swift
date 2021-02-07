//
//  AppDelegate.swift
//  FeedsServiceClient
//
//  Created by Gabriel Theodoropoulos.
//  Copyright © 2020 AppCoda. All rights reserved.
//

import Cocoa

extension NSImage {
    func tint(color: NSColor) -> NSImage {
        return NSImage(size: size, flipped: false) { (rect) -> Bool in
            color.set()
            rect.fill()
            self.draw(in: rect, from: NSRect(origin: .zero, size: self.size), operation: .destinationIn, fraction: 1.0)
            return true
        }
    }
}

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    @IBOutlet weak var menu: NSMenu?
    @IBOutlet weak var firstMenuItem: NSMenuItem?
    
    var statusItem: NSStatusItem?
    var dateTimeView: DateTimeView?
    
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }


    
    override func awakeFromNib() {
        super.awakeFromNib()

        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        // statusItem?.button?.title = "FeedsServiceClient"
        
        let menuImage = NSImage(named: "MenuIcon")
        menuImage?.isTemplate = true
      
        let selectedColor = NSColor.black
        let unselectedColor: NSColor
        if #available(OSX 10.14, *) {
          unselectedColor = selectedColor.withSystemEffect(.disabled)
        } else {
          unselectedColor = selectedColor.withAlphaComponent(0.2)
        }

        statusItem?.button?.image = menuImage?.tint(color: true
                                                    ? selectedColor : unselectedColor)
//        statusItem?.button?.image = menuImage
        
        if let menu = menu {
            statusItem?.menu = menu
            menu.delegate = self
        }
        
        if let item = firstMenuItem {
            dateTimeView = DateTimeView(frame: NSRect(x: 0.0, y: 0.0, width: 250.0, height: 170.0))
            item.view = dateTimeView
        }
    }
 
    
    @IBAction func showPreferences(_ sender: Any) {
        let storyboard = NSStoryboard(name: "Main", bundle: nil)
        guard let vc = storyboard.instantiateController(withIdentifier: .init(stringLiteral: "preferencesID")) as? ViewController else { return }
        
        let window = NSWindow(contentViewController: vc)
        window.makeKeyAndOrderFront(nil)
    }
}



extension AppDelegate: NSMenuDelegate {
    func menuWillOpen(_ menu: NSMenu) {
        dateTimeView?.startTimer()
    }
    
    
    func menuDidClose(_ menu: NSMenu) {
        dateTimeView?.stopTimer()
    }
}
