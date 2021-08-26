# SpeedoBLE for the Watchy smartphone

This needs an app to provide data to it.  Currently it only works with this iOS app https://github.com/punkoffice/SpeedoBLE-iOS

### Connecting
Make sure Watchy is in "waiting" state before running the iOS app.  If Watchy is in "sleeping" state, press any of the buttons to wake it up.
If the Watchy says "Disconnected" on its screen, its still acting as a bluetooth client and needs to be fully disconnected by forgetting the device in your bluetooth settings.

When you first open the app on your iPhone you should get a "Bluetooth Pairing Request" prompt.  This sets up Watchy as a server which receives custom data (time and GPS) from the iPhone.  A few seconds like you should get a "Allow Watchy to display your iPhone notifications" prompt.  This sets up Watchy as a client to iOS notification services so it can receive text messages.  If this prompt fails to show then Watchy may already be connected in the list of bluetooth devices and will need to be forgotten from your iPhone settings before trying this again.

### Disconnecting
You will need to close the iPhone app (swipe it away) to disconnect the server then "forget the device" from your bluetooth settings to disconnect the client.

### Text message notifications
Text messages will arrive from the iPhone.  It will disappear after a few seconds or if you press the bottom-right button.
If you don't see the message in time and would like to re-show it, you can flick the wrist to re-show it.  
The "flick" is done by first bringing your wrist up and pointing the Watchy screen to the sky (like how you would normally rotate your wrist to see the watch face).
Then quickly rotate your wrist outwards (away from your body) then quickly rotate it back.  The Watchy should vibrate when it registers this wrist flick.

You can tell the difference between a text message's first arrival and re-shows.  The first message will have an "x" on the screen next to the bottom-right button
which you can press to hide it instantly.  The re-shows only disappear after a few seconds.

## To Do

### Disconnection issues
The bluetooth pairing disconnects with the iPhone app sometimes.  
This seems to happen when I've stopped moving, so maybe there's some kind of idling timeout that happens when there's no GPS data sent to the Watchy.  
I'll have to test this out more.  Weird thing is, its still connected as a client to iOS services and will receive text msg notifications.
This makes me think that its simply my app causing this problem.

### Distance travelled
Haven't tested this out properly yet.  Might not work as expected.

### Time
This grabs the current time from the iPhone on connection then uses its RTC from there on.  There might be some issues with this.  
I previously noticed the time jumping around soon after connection, as if something was overwriting that piece of memory.

### Reconnection
Having to disconnect the server and also the client is kind of annoying.  It'd be neat to find out how to also disconnect the BLE client from within the Watchy app or the iPhone app.

### Android
It would be cool to connect this to Android devices.  If anyone has some good Android dev skills, pls give it a go.
