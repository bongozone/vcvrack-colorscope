# Colorscope

A Scope plugin for [VCVRack](https://github.com/VCVRack/) where drawing colors follow the wire colors of the inputs. Forked from [Fundamental](https://github.com/VCVRack/Fundamental)'s scope, based on an [unaccepted pull request](https://github.com/VCVRack/Fundamental/pull/34).

<img width="217" alt="scope2" src="https://user-images.githubusercontent.com/15206/35821358-c6874732-0a76-11e8-88cb-426ecec24ece.png"><img width="232" alt="scope1" src="https://user-images.githubusercontent.com/15206/35821359-c691df08-0a76-11e8-962a-7e3fb568c38b.png">

If the wires are the same color, RGB values are shuffled, such that neither X nor Y will match the input color.

<img width="252" alt="scope5" src="https://user-images.githubusercontent.com/15206/35821355-c651d354-0a76-11e8-919b-8439481d6a9f.png"><img width="241" alt="scope4" src="https://user-images.githubusercontent.com/15206/35821356-c66082fa-0a76-11e8-9885-0af299f96e41.png"><img width="226" alt="scope3" src="https://user-images.githubusercontent.com/15206/35821357-c679f3f2-0a76-11e8-834d-03f93326dacd.png">

## Building

Follow the build instructions for [VCV Rack](https://github.com/VCVRack/Rack).
The `master` branch of this repo builds against the `master` branch of Rack.
To build for a previous version, use `git tag` to find a compatible version and `git checkout [TAG]` to check out the source for that version.


## License

Source code licensed under [BSD-3-Clause](LICENSE.txt) by [Andrew Belt](https://andrewbelt.name/)

Brutally modified by [Jon Williams](https://jonwillia.ms)

_Does not use the Fundamental plugin's copyrighted panel graphics! These screenshots are from the pull request._
