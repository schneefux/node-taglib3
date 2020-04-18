# node-taglib3

A rewrite of [node-taglib2](https://github.com/voltraco/node-taglib2) for Node 10 to 13 and taglib 1.11.1 with support for non-standard properties and a non-blocking API.

## Installation

### OSX/Linux

You need to have installed a proper C/C++ compiler toolchain, like GCC (For OSX please download [Xcode and Command Line Tools](https://developer.apple.com/xcode/)).

### Windows

You need to have Visual C++ Build Environment setup, which you can download as a standalone [Visual C++ Build Tools](http://landinghub.visualstudio.com/visual-cpp-build-tools) package or get it as part of [Visual Studio 2015](https://www.visualstudio.com/products/visual-studio-community-vs).

## Usage
For example, with electron:

```
ELECTRON=1 npm install
```

### Writing tags

Specify an object of tag names and tag entries to (over) write. Tag entries must be arrays of strings. Not all file formats support multiple entries.
See the [taglib documentation](https://taglib.org/api/classTagLib_1_1PropertyMap.html) for details.


```js
const taglib = require('taglib2')

const props = {
  artist: ['Howlin\' Wolf'],
  title: ['Evil is goin\' on'],
  my_own_special_attribute: ['datadatadata']
}

// sync
taglib.writeTagsSync('file.mp3', props)
// async
taglib.writeTags('file.mp3', props, (error, data) => console.log(error, data))
```

### Reading tags

```js
const taglib = require('taglib2')
const path = require('path')
// sync
const tags = taglib.readTagsSync('file.mp3')
// async
const tags = taglib.readTags('file.mp3', (error, data) => console.log(error, data))
```

```json
{
  "ARTIST": ["Howlin' Wolf"],
  "TITLE": ["Howlin' Wolf"],
  "MY_OWN_SPECIAL_ATTRIBUTE": ["datadatadata"]
}
```
