const binding = require('./build/Release/taglib3.node')

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

exports.writeId3Tags = (path, options, callback) => {
  path = resolve(path)
  return lock.acquire(path, (cb) => binding.writeId3Tags(path, options, cb), callback)
}

exports.writeId3TagsSync = (path, options) => {
  path = resolve(path)
  return binding.writeId3TagsSync(path, options)
}

exports.readTags = (path, callback) => {
  path = resolve(path)
  return lock.acquire(path, (cb) => binding.readTags(path, cb), callback)
}

exports.readTagsSync = path => {
  path = resolve(path)
  return binding.readTagsSync(path)
}

exports.readId3Tags = (path, callback) => {
  path = resolve(path)
  return lock.acquire(path, (cb) => binding.readId3Tags(path, cb), callback)
}

exports.readId3TagsSync = path => {
  path = resolve(path)
  return binding.readId3TagsSync(path)
}

exports.readAudioProperties = (path, callback) => {
  path = resolve(path)
  return lock.acquire(path, (cb) => binding.readAudioProperties(path, cb), callback)
}

exports.readAudioPropertiesSync = path => {
  path = resolve(path)
  return binding.readAudioPropertiesSync(path)
}
