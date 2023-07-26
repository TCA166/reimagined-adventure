# reimagined-adventure

C UNIX program for controlling file types in directory

## Usage

The process will delete every file that isn't whitelisted in the config file.

### Launch

```Bash
./process <path to config> <args>
```

Process supports 3 additional optional arguments:

- -r: the recursive flag will make the process apply the rules of a given directory to it's subdirectories
- -v: the verbose flag will make the process notify the user of it's actions
- -c: the confirm flag will make the process check with the user on the deletions (Very useful for debugging config files)

### Config file

There are two kinds of lines in the config file:

- directory definition lines: these provide the path to the directory
- whitelist lines: these define which files the program should ignore

Each whitelist line starts with either a \t character (tab) or a number of spaces.
All whitelist lines that follow a dir def line are considered to apply to only this one directory.
Whitelist lines should either contain filenames that should be ignored, or MIME types that should be ignored.
You can find what MIME type a file has by using this command:

```Bash
file --mime-type <filename>
```
