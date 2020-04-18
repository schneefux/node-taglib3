'use strict'
const fs = require('fs')
const taglib3 = require('../index')
const test = require('tape')
const path = require('path')
const http = require('http')

const FIXTURES_PATH = path.join(__dirname, '/fixtures')

;(function setup () {
  const flacfilesource = [
    'http://www.eclassical.com',
    '/custom/eclassical/files/BIS1447-002-flac_24.flac'
  ].join('')

  const flacfile = FIXTURES_PATH + '/classical å æ ø ö ä ù ó ð.flac'

  fs.stat(flacfile, (err, stat) => {
    if (!err) return onReady()

    const ws = fs.createWriteStream(flacfile)
    http.get(flacfilesource, res => {
      res.pipe(ws)
      ws.on('end', onReady)
    })
  })
})()

function onReady () {
  test('sync write/read', assert => {
    const audiopath = FIXTURES_PATH + '/sample-output å æ ø ö ä ù ó ð.mp3'
    fs.writeFileSync(audiopath, fs.readFileSync(FIXTURES_PATH + '/sample.mp3'))

    assert.ok(!!fs.statSync(audiopath))

    assert.throws(() => {
      taglib3.writeTagsSync()
    }, 'not enough arguments')

    const r = taglib3.writeTagsSync(audiopath, {
      artist: ['å æ ø ö ä ù ó ð ärtist']
    })

    assert.ok(r)

    assert.throws(() => {
      taglib3.readTagsSync()
    }, 'not enough arguments')

    const tags = taglib3.readTagsSync(audiopath)

    assert.equal(tags.ARTIST[0], 'å æ ø ö ä ù ó ð ärtist')
    assert.end()
  })
}
