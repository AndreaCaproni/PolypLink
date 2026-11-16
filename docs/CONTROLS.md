# Controls

The Link remote is read via SDL and translated into arrows, A, B, Y and Start.
A USB keyboard works the same way.

## Home

| Key | Action |
|---|---|
| arrows | move between rows and cards |
| **A** | open the title detail page |
| **B** | back; on the home screen it exits the app |
| **up** from the first row | go to the top bar |

## Top bar

Home · Favorites · Account · Settings

| Key | Action |
|---|---|
| left/right | choose the entry |
| **A** | open |
| **down** | return to the content |
| **B** | cancel |

## Title detail (movie / episode)

| Key | Action |
|---|---|
| up/down | actions → settings |
| left/right | on buttons: choose; on settings: change the value |
| **A** | confirm |
| **B** | back |

With resume progress, *Resume* and *Play from beginning* are shown.

## Series and seasons

**Series detail**

| Key | Action |
|---|---|
| left/right | *Play* · *Seasons* · *Mark watched* · *Favorite* |
| **A** | play the next episode / open seasons / toggle |
| **B** | back |

**Seasons grid**

| Key | Action |
|---|---|
| arrows | choose the season |
| **A** | open the season page |
| **Y** | mark the season watched / unwatched |
| **B** | back to the series |

**Season page**

| Key | Action |
|---|---|
| **up** from the first episode | go to the buttons (*Play from beginning*, *Mark watched*, *Favorite*) |
| left/right | on buttons: choose; **A** activates |
| **down** | back to the episode list |
| **A** on an episode | open the episode detail |
| **B** | back to the seasons grid |

## Server selection

On first start the app searches for servers (UDP + LAN scan). If a server is
already saved it appears at the top: **A** to use it. **X/R** repeats the
search, **B** exits.

## Player

The OSD (seek bar + buttons) stays for 5 seconds, then fades. Any key brings it
back.

### On the seek bar

| Key | Action |
|---|---|
| left / right tap | move the cursor by **-10 s / +30 s** |
| hold the analog stick | the step grows: **1 min**, then **3 min** (maximum) |
| **A** | confirm the jump |
| **up** | hide the OSD immediately (video only) |
| **down** | go to the buttons |

The jump starts **only when you press A**. Moving the cursor or waiting does not
touch the stream; **down**, **up** or **B** cancels a pending jump. The OSD stays
visible while the seek bar has focus.

### On the buttons

| Key | Action |
|---|---|
| left/right | change button |
| **A** | activate |
| **up** | back to the seek bar |
| **down** | back to the seek bar |

Buttons: **-10 s**, **play/pause**, **+30 s**, **Audio**, **Subtitles**,
**Quality**, **Stats**.

### Other keys

| Key | Action |
|---|---|
| **B** | hide the OSD; if already hidden, exit the player |
| **Y** | open/close the settings panel |
| **Start / Menu** | transcoding statistics |

## USB keyboard

Arrows, **Enter** (A), **Esc** (B), **Y**, **Menu**. In text fields the physical
keyboard types directly.
