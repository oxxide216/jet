# Jet

**!!! PROJECT IS WORK IN PROGRESS !!!**

Jet is a lightweight text/code editor, written from scratch in C using Vulkan and a bunch of my own libraries.

![This README.md in Jet inself](./screenshots/0.png)

## Advanced features

Apart from basic editing features, jwrap has these additional ones:

- jwrap: after launching Jet, run jwrap with your compilation command. Jet will get all errors/warnings/infos, display them in a separate menu and inline. On save - automatic re-run.
- Autocompletion: collects words from all open buffers, dynamically updates, works fine on a file with 36938 lines of code and many prefix duplications.

## Quick start

Jet uses [my custom build system](https://github.com/oxxide216/nsb).

```shell
git clone --recursive https://github.com/oxxide216/jet
cd jet
cc -o nsb nsb.c
./nsb
./jet
```

## Why?

Jet is not a VS Code, Vim, Neovim, Emacs or *yout favourite IDE* killer.
I just wanted to create something that I could use daily myself.
This is why main roadmap for Jet - features that *I* need daily.
I don't want my editor to be Electron bloat. I don't need many IDE features.
I need a simple and fast editor. That's it.

## What does `Jet` mean?

Jets are things in space that come from quasars and pulsars.
They are very fast, powerful and beautiful.
This is how I want Jet to be: fast, powerful and beautiful.
