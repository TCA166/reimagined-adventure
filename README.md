# reimagined-adventure

C UNIX daemon for controlling file types in directory

## Usage

Provide a path to a config file, and the process will create a daemon that periodically checks that config.
If that config cannot be found the daemon dies, else the config will be reexamined and the provided directories cleansed of files that don't fit with the whitelist in the config

### Launch

```Bash
./daemon <path to config>
```

### Config file

The syntax is quite simple really.

```Text
./test-directory
    file
    some other kind of file
./other
    file
```
