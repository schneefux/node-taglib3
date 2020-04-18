const binding = require('./build/Release/taglib3')
// for some reason, the binding only works reliably with absolute paths
const resolve = require('path').resolve

exports.writeTags = (path, options, callback) => {
  path = resolve(path)
  return binding.writeTags(path, options, callback)
}

exports.writeTagsSync = (path, options) => {
  path = resolve(path)
  return binding.writeTagsSync(path, options)
}

exports.readTags = (path, callback) => {
  path = resolve(path)
  return binding.readTags(path, callback)
}

exports.readTagsSync = path => {
  path = resolve(path)
  return binding.readTagsSync(path)
}
