const binding = require('./build/Release/taglib3')

// FIXME implement this in native code (concurrent file access does not work)
const AsyncLock = require('async-lock')
const lock = new AsyncLock({ timeout: 60000 })

// for some reason, the binding only works reliably with absolute paths
const resolve = require('path').resolve

exports.writeTags = (path, options, callback) => {
  path = resolve(path)
  return lock.acquire(path, (cb) => binding.writeTags(path, options, cb), callback)
}

exports.writeTagsSync = (path, options) => {
  path = resolve(path)
  return binding.writeTagsSync(path, options)
}

exports.readTags = (path, callback) => {
  path = resolve(path)
  return lock.acquire(path, (cb) => binding.readTags(path, cb), callback)
}

exports.readTagsSync = path => {
  path = resolve(path)
  return binding.readTagsSync(path)
}
