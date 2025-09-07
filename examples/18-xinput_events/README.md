# XInput Events Monitor

This example addon demonstrates how to monitor and log XInput gamepad events in ReShade. It shows how to use the `xinput_get_state` event to track controller input changes in real-time.

## Features

- **Real-time Gamepad Monitoring**: Tracks all XInput controller events including button presses, releases, trigger movements, and thumbstick changes
- **Multi-Controller Support**: Monitors up to 4 controllers simultaneously
- **Visual Overlay**: Displays current controller states and input changes in a ReShade overlay
- **Event Logging**: Logs all input events with timestamps and controller identification
- **Connection Status**: Shows which controllers are connected/disconnected

## How It Works

The addon registers for the `reshade::addon_event::xinput_get_state` event, which is called whenever the game queries the state of an XInput controller. The addon:

1. **Intercepts XInput calls**: Captures all `XInputGetState` calls made by the game
2. **Tracks state changes**: Compares current state with previous state to detect changes
3. **Logs events**: Records button presses/releases, trigger movements, and thumbstick changes
4. **Displays overlay**: Shows real-time controller status and input information

## Usage

1. **Install the addon**: Copy `xinput_events.addon64` to your game directory alongside ReShade
2. **Launch the game**: Start a game that uses XInput controllers
3. **Open ReShade overlay**: Press the ReShade overlay key (default: Home)
4. **View the monitor**: Look for "XInput Events Monitor" in the addon list
5. **Connect controllers**: Plug in Xbox controllers or other XInput-compatible gamepads
6. **Test input**: Press buttons, move thumbsticks, and pull triggers to see real-time feedback

## Overlay Features

- **Controller Status**: Shows which controllers (0-3) are connected
- **Current Button States**: Displays all currently pressed buttons
- **Trigger Values**: Shows left and right trigger positions as percentages
- **Thumbstick Values**: Displays normalized thumbstick positions (-1.0 to 1.0)
- **Event Log**: Scrollable list of recent input events
- **Logging Toggle**: Enable/disable event logging
- **Clear Log**: Clear the event log

## Technical Details

### Event Handling
```cpp
static bool on_xinput_get_state(uint32_t dwUserIndex, void* pState)
{
    // Intercept XInputGetState calls
    // Track state changes
    // Log events
    return false; // Don't modify the state
}
```

### State Tracking
- Tracks button states using bitwise operations
- Monitors trigger values with deadzone handling
- Tracks thumbstick positions with deadzone filtering
- Compares packet numbers to detect state changes

### Button Mapping
The addon maps XInput button constants to readable names:
- `XINPUT_GAMEPAD_A` → "A"
- `XINPUT_GAMEPAD_B` → "B"
- `XINPUT_GAMEPAD_X` → "X"
- `XINPUT_GAMEPAD_Y` → "Y"
- `XINPUT_GAMEPAD_DPAD_UP` → "DPAD_UP"
- And more...

## Building

This example is included in the ReShade Examples solution. To build:

1. Open `examples/Examples.sln` in Visual Studio
2. Build the `18-xinput_events` project
3. The output will be `xinput_events.addon64`

## Dependencies

- **XInput**: Windows XInput library for gamepad input
- **ReShade API**: ReShade addon framework
- **ImGui**: For the overlay interface

## Example Use Cases

- **Input Debugging**: Debug gamepad input issues in games
- **Controller Testing**: Test controller functionality and responsiveness
- **Input Recording**: Log input sequences for analysis
- **Accessibility**: Monitor controller input for accessibility tools
- **Game Development**: Understand how games handle XInput

## Notes

- The addon only monitors XInput calls, not DirectInput or other input APIs
- Some games may not use XInput, so the addon won't detect input in those cases
- The overlay is only visible when ReShade is active and the overlay is open
- Event logging can be disabled to improve performance if needed
