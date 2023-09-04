# reimagined-adventure

C UNIX programs for controlling file types in directory

## Program

The program version will delete every file that isn't whitelisted in the config file once and then shut off.

```Bash
./process <path to config> ...
```

Process supports 3 additional optional arguments:

- -r: the recursive flag will make the process apply the rules of a given directory to it's subdirectories
- -v: the verbose flag will make the process notify the user of it's actions
- -c: the confirm flag will make the process check with the user on the deletions (Very useful for debugging config files)

Ideally you use this utility to setup the daemon yourself using built in UNIX tools for that like cron.

## Daemon

The daemon program will create a new daemon process called "directory monitor" that will monitor directories in config using the inotify API.
Should a new file be added to a monitored directory, the directory-monitor will scan the directory where a change was detected and act accordingly.

```Bash
./daemon <path to config file> ...
```

Daemon supports three additional arguments:

- -r:recursive override that will force the daemon to act recursively globally
- -v:verbose override
- -l $level:will redirect the syslog output of daemon to syslog local facility with the given number (LOG_LOCAL0 etc).
    Can be used to redirect the log to file when combined with a custom [syslog.conf](https://linux.die.net/man/5/syslog.conf).
    By default the daemon uses the LOG_DAEMON facility.

The daemon uses the syslog to notify the user of it's status.
After each received set of event from inotify the daemon will reload it's config, so feel free to modify the config freely.
Apart from the config reloading itself, a SIGUSR1 signal to the daemon will force an immediate reload.
By default the daemon logs when it activates, shutdowns gracefully (detects the config is missing), is terminated or crashes due to an error.
With the verbose flag the individual actions are also logged.
The daemon also creates a pid file at /var/run/directory-monitor.pid

## Config file

There are three kinds of lines in the config file:

- directory definition lines: these provide the path to the directory
- whitelist lines: these define which files the program should ignore
- variable lines: these set a value for the current scope

Each whitelist line starts with either a \t character (tab) or a number of spaces.
All whitelist lines that follow a dir def line are considered to apply to only this one directory.
Whitelist lines should either contain filenames that should be ignored, or MIME types that should be ignored.
You can find what MIME type a file has by using this command:

```Bash
file --mime-type <filename>
```

Variable lines set the variable for the current scope.
The scope is determined with indentation just like whitelist lines.
So far only 3 variables are supported.

1. recursive
2. verbose
3. move: if provided, the files in this directory won't be deleted, instead moved to the provided path.

Following the variable name a : followed by a value must be provided.
The config format also supports comments, that are indicated by a single '#' character. Everything after that character is ignored.
Here's an example config file:

```config
$move:./moved
./onlyText
    $move:./nonText
    $recursive:true
    $verbose:true
    text/plain
./onlyMusic
    audio/mpeg #all of these audio files will remain
    sound.ogg #and this one specific ogg file will remain
```
