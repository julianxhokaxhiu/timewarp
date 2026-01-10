![License](https://img.shields.io/github/license/julianxhokaxhiu/timewarp) ![Overall Downloads](https://img.shields.io/github/downloads/julianxhokaxhiu/timewarp/total?label=Overall%20Downloads) ![Latest Stable Downloads](https://img.shields.io/github/downloads/julianxhokaxhiu/timewarp/latest/total?label=Latest%20Stable%20Downloads&sort=semver) ![Latest Canary Downloads](https://img.shields.io/github/downloads/julianxhokaxhiu/timewarp/canary/total?label=Latest%20Canary%20Downloads) ![GitHub Actions Workflow Status](https://github.com/julianxhokaxhiu/timewarp/actions/workflows/main-0.1.0.yml/badge.svg?branch=master)

# timewarp

Fake Windows Time for applications

# Why

I wanted to build an easy to use solution to be able to unlock time-based content ( eg. Christmas events or Halloween events ) without the need to change the clock on the entire PC.

This solution allows to fake the time by simply overriding the system calls used by the game and returning whatever is configured in [`timewarp.toml`](misc/timewarp.toml).

# How it works

Once the game process starts, timewarp will begin counting forward like the normal clock would do, but starting from `custom_start_time` defined in [`timewarp.toml`](misc/timewarp.toml).

> **PLEASE NOTE:** If you close and re-open the game, the timer will restart at the configured time, not at the last time you closed the game. **This is important to know** as save files for eg. might be overridden if time is used for the name, as the clock will "restart".

# How to use

timewarp acts as a proxy dll on top of the game you want to use it.

> **PLEASE NOTE:** This solution has been tested on top of Unreal Engine 5.x games so far

1. Download the latest release from https://github.com/julianxhokaxhiu/timewarp/releases
2. Extract its content next to the game exe
3. Configure the time you want the game to use in `timewarp.toml`
4. Run the game and enjoy!

# License

timewarp is released under GPLv3 license. You can get a copy of the license here: COPYING.TXT

If you paid for timewarp, remember to ask for a refund from the person who sold you a copy. Also make sure you get a copy of the source code (if it was provided as binary only).

If the person who gave you this copy refuses to give you the source code, report it here: https://www.gnu.org/licenses/gpl-violation.html

All rights belong to their respective owners.
