"""Music4All companion app entrypoint.

This repo historically shipped the desktop companion as `instapod.py`.
For the rebrand, Winamp scripts/tools call `music4all.py`.
"""

from instapod import main


if __name__ == "__main__":
    main()
