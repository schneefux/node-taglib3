#include <v8.h>
#include <nan.h>
#include <node.h>
#include <node_buffer.h>

#define NDEBUG
#define TAGLIB_STATIC
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/generalencapsulatedobjectframe.h>

// TagLib string -> V8 string
v8::Local<v8::String> TagLibStringToString(TagLib::String s) {
  return Nan::New<v8::String>(s.toCString(true)).ToLocalChecked();
}

// V8 string -> TagLib string
TagLib::String StringToTagLibString(v8::Local<v8::String> s) {
  return TagLib::String(*Nan::Utf8String(s), TagLib::String::UTF8);
}

// TagLib String -> TagLib FileName
TagLib::FileName StringToFileName(TagLib::String s) {
#ifdef _WIN32
  return s.toCWString();
#else
  return s.toCString(true);
#endif
}

bool ValidatePath(v8::Local<v8::Value> path) {
  if (!path->IsString()) {
    Nan::ThrowTypeError("Expected a string");
    return false;
  }
  return true;
}

bool ValidateFile(TagLib::FileRef f) {
  if (f.isNull()) {
    Nan::ThrowTypeError("Could not parse file");
    return false;
  }
  return true;
}

bool ValidateProperties(v8::Local<v8::Value> props) {
  // TODO add more type checking
  if (!props->IsObject()) {
    Nan::ThrowTypeError("Expected an object");
    return false;
  }
  return true;
}

bool ValidateCallback(v8::Local<v8::Value> callback) {
  if (!callback->IsFunction()) {
    Nan::ThrowTypeError("Expected a callback");
    return false;
  }
  return true;
}

// https://github.com/taglib/taglib/blob/79bb1428c0482966cdafd9b6e1127e98b4637fbf/taglib/mpeg/id3v2/id3v2frame.cpp#L92
TagLib::ByteVector textDelimiter(TagLib::String::Type t)
{
  if(t == TagLib::String::UTF16 || t == TagLib::String::UTF16BE || t == TagLib::String::UTF16LE)
    return TagLib::ByteVector(2, '\0');
  else
    return TagLib::ByteVector(1, '\0');
}

// https://github.com/taglib/taglib/blob/79bb1428c0482966cdafd9b6e1127e98b4637fbf/taglib/mpeg/id3v2/id3v2frame.cpp#L265
TagLib::String readStringField(const TagLib::ByteVector &data, TagLib::String::Type encoding, int *position)
{
  int start = 0;

  if(!position)
    position = &start;

  TagLib::ByteVector delimiter = textDelimiter(encoding);

  int end = data.find(delimiter, *position, delimiter.size());

  if(end < *position)
    return TagLib::String();

  TagLib::String str = TagLib::String(data.mid(*position, end - *position), encoding);

  *position = end + delimiter.size();

  return str;
}

// map -> v8 object
v8::Local<v8::Object> MapToObject(TagLib::Map<TagLib::String, TagLib::String> map, v8::Local<v8::Context> context) {
  v8::Local<v8::Object> obj = Nan::New<v8::Object>();

  for (TagLib::Map<TagLib::String, TagLib::String>::ConstIterator i = map.begin(); i != map.end(); ++i) {
    obj->Set(context, TagLibStringToString(i->first), TagLibStringToString(i->second));
  }

  return obj;
}

// property map -> v8 object with array as values
v8::Local<v8::Object> PropertyMapToObject(TagLib::PropertyMap map, v8::Local<v8::Context> context) {
  v8::Local<v8::Object> obj = Nan::New<v8::Object>();

  for (TagLib::PropertyMap::ConstIterator i = map.begin(); i != map.end(); ++i) {
    v8::Local<v8::Array> array = Nan::New<v8::Array>(i->second.size());

    int index = 0;
    for (TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j) {
      array->Set(context, index++, TagLibStringToString(*j));
    }

    obj->Set(context, TagLibStringToString(i->first), array);
  }

  return obj;
}

// v8 object with arrays as values into property map
TagLib::PropertyMap ObjectToPropertyMap(v8::Local<v8::Object> props, v8::Local<v8::Context> context) {
  v8::Local<v8::Array> property_names = props->GetOwnPropertyNames(context).ToLocalChecked();
  TagLib::PropertyMap map;

  for (int i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::String> key = property_names->Get(context, i).ToLocalChecked().As<v8::String>();

    TagLib::StringList list = TagLib::StringList();

    v8::Local<v8::Array> values = props->Get(context, key).ToLocalChecked().As<v8::Array>();
    for (int j = 0; j < values->Length(); ++j) {
      v8::Local<v8::String> value = values->Get(context, j).ToLocalChecked().As<v8::String>();
      list.append(StringToTagLibString(value));
    }

    map.insert(StringToTagLibString(key), list);
  }

  return map;
}

// v8 object into map
TagLib::Map<TagLib::String, TagLib::String> ObjectToMap(v8::Local<v8::Object> props, v8::Local<v8::Context> context) {
  v8::Local<v8::Array> property_names = props->GetOwnPropertyNames(context).ToLocalChecked();
  TagLib::Map<TagLib::String, TagLib::String> map;

  for (int i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::String> key = property_names->Get(context, i).ToLocalChecked().As<v8::String>();
    v8::Local<v8::String> value = props->Get(context, key).ToLocalChecked().As<v8::String>();
    map.insert(StringToTagLibString(key), StringToTagLibString(value));
  }

  return map;
}

// merge two property maps, replacing existing map1 entries with the corresponding map1 entry
TagLib::PropertyMap MergePropertyMaps(TagLib::PropertyMap map1, TagLib::PropertyMap map2) {
  TagLib::PropertyMap map; // = new TagLib::PropertyMap();

  for (TagLib::PropertyMap::ConstIterator i = map1.begin(); i != map1.end(); ++i) {
    map.replace(i->first, i->second);
  }
  for (TagLib::PropertyMap::ConstIterator i = map2.begin(); i != map2.end(); ++i) {
    map.replace(i->first, i->second);
  }

  return map;
}

TagLib::PropertyMap ReadTags(TagLib::FileRef f) {
  return f.file()->properties();
}

TagLib::Map<TagLib::String, TagLib::String> ReadId3Tags(TagLib::FileRef f) {
  TagLib::Map<TagLib::String, TagLib::String> map;

  if (TagLib::MPEG::File* mpgfile = dynamic_cast<TagLib::MPEG::File*>(f.file())) {
    TagLib::ID3v2::Tag* id3v2 = mpgfile->ID3v2Tag();
    if (id3v2 == nullptr) {
      return map;
    }

    const TagLib::ID3v2::FrameList framelist = id3v2->frameListMap()["GEOB"];
    for (auto it = framelist.begin(); it != framelist.end(); it++) {
      TagLib::ID3v2::GeneralEncapsulatedObjectFrame* frame = dynamic_cast<TagLib::ID3v2::GeneralEncapsulatedObjectFrame*>(*it);

      TagLib::ByteVector data;
      data.append(frame->mimeType().data(TagLib::String::Latin1));
      data.append(textDelimiter(TagLib::String::Latin1));
      data.append(frame->fileName().data(TagLib::String::Latin1));
      data.append(textDelimiter(TagLib::String::Latin1));
      data.append(frame->description().data(TagLib::String::Latin1));
      data.append(textDelimiter(TagLib::String::Latin1));
      data.append(frame->object());

      TagLib::ByteVector b64 = data.toBase64();
      map.insert(frame->description(), TagLib::String(b64));
    }
  }

  return map;
}

TagLib::Map<TagLib::String, TagLib::String> ReadAudioProperties(TagLib::FileRef f) {
  TagLib::Map<TagLib::String, TagLib::String> map;

  map.insert(TagLib::String("bitrate"), TagLib::String(std::to_string(f.audioProperties()->bitrate())));
  map.insert(TagLib::String("channels"), TagLib::String(std::to_string(f.audioProperties()->channels())));
  map.insert(TagLib::String("length"), TagLib::String(std::to_string(f.audioProperties()->length())));
  map.insert(TagLib::String("samplerate"), TagLib::String(std::to_string(f.audioProperties()->sampleRate())));

  return map;
}

void WriteTags(TagLib::FileRef f, TagLib::PropertyMap map) {
  if (map.size() > 0) {
    f.file()->setProperties(map);
    f.file()->save();
  }
}

void WriteId3Tags(TagLib::FileRef f, TagLib::Map<TagLib::String, TagLib::String> map) {
  if (TagLib::MPEG::File* mpgfile = dynamic_cast<TagLib::MPEG::File*>(f.file())) {
    TagLib::ID3v2::Tag* id3v2 = mpgfile->ID3v2Tag(true);

    for (TagLib::Map<TagLib::String, TagLib::String>::ConstIterator i = map.begin(); i != map.end(); ++i) {
      // delete any existing GEOB with this key as description
      const TagLib::ID3v2::FrameList geobs = id3v2->frameListMap()["GEOB"];
      for (auto it = geobs.begin(); it != geobs.end(); it++) {
        TagLib::ID3v2::GeneralEncapsulatedObjectFrame* frame = dynamic_cast<TagLib::ID3v2::GeneralEncapsulatedObjectFrame*>(*it);
        if (frame->description() == i->first) {
          id3v2->removeFrame(frame, true);
          break;
        }
      }

      if (i->second.size() > 0) {
        // append a GEOB with this key
        TagLib::ByteVector b64 = TagLib::ByteVector::fromCString(i->second.toCString());
        TagLib::ByteVector data = TagLib::ByteVector::fromBase64(b64);
        TagLib::ID3v2::GeneralEncapsulatedObjectFrame *geob = new TagLib::ID3v2::GeneralEncapsulatedObjectFrame();

        int pos = 0;
        geob->setMimeType(readStringField(data, TagLib::String::Latin1, &pos));
        geob->setFileName(readStringField(data, TagLib::String::Latin1, &pos));
        geob->setDescription(readStringField(data, TagLib::String::Latin1, &pos));
        geob->setObject(data.mid(pos));

        id3v2->addFrame(geob);
      }
    }

    mpgfile->save(0x0002, true, 3); // save as ID3 2.3, strip ID3v1 & APE
  }
}

class ReadTagsWorker : public Nan::AsyncWorker {
  public:
    ReadTagsWorker(Nan::Callback *callback, TagLib::String path)
      : Nan::AsyncWorker(callback), path(path) {}
  ~ReadTagsWorker() { }

  void Execute() {
    TagLib::FileRef f(StringToFileName(path), false);
    if (f.isNull()) {
      this->SetErrorMessage("Could not parse file");
      return;
    }

    this->result = ReadTags(f);
  }

  void HandleOKCallback() {
    v8::Local<v8::Context> context = Nan::GetCurrentContext();

    v8::Local<v8::Object> obj = PropertyMapToObject(this->result, context);
    v8::Local<v8::Value> argv[2] = {
      Nan::Null(),
      obj
    };

    callback->Call(2, argv, async_resource);
  }

  void HandleErrorCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::New<v8::String>(this->ErrorMessage()).ToLocalChecked(),
      Nan::Null()
    };

    callback->Call(2, argv, async_resource);
  }

  private:
    TagLib::String path;
    TagLib::PropertyMap result;
};

class ReadAudioPropertiesWorker : public Nan::AsyncWorker {
  public:
    ReadAudioPropertiesWorker(Nan::Callback *callback, TagLib::String path)
      : Nan::AsyncWorker(callback), path(path) {}
  ~ReadAudioPropertiesWorker() { }

  void Execute() {
    TagLib::FileRef f(StringToFileName(path), true, TagLib::AudioProperties::Fast);
    if (f.isNull()) {
      this->SetErrorMessage("Could not parse file");
      return;
    }

    this->result = ReadAudioProperties(f);
  }

  void HandleOKCallback() {
    v8::Local<v8::Context> context = Nan::GetCurrentContext();

    v8::Local<v8::Object> obj = MapToObject(this->result, context);
    v8::Local<v8::Value> argv[2] = {
      Nan::Null(),
      obj
    };

    callback->Call(2, argv, async_resource);
  }

  void HandleErrorCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::New<v8::String>(this->ErrorMessage()).ToLocalChecked(),
      Nan::Null()
    };

    callback->Call(2, argv, async_resource);
  }

  private:
    TagLib::String path;
    TagLib::Map<TagLib::String, TagLib::String> result;
};

class ReadId3TagsWorker : public Nan::AsyncWorker {
  public:
    ReadId3TagsWorker(Nan::Callback *callback, TagLib::String path)
      : Nan::AsyncWorker(callback), path(path) {}
  ~ReadId3TagsWorker() { }

  void Execute() {
    TagLib::FileRef f(StringToFileName(path), false);
    if (f.isNull()) {
      this->SetErrorMessage("Could not parse file");
      return;
    }

    this->result = ReadId3Tags(f);
  }

  void HandleOKCallback() {
    v8::Local<v8::Context> context = Nan::GetCurrentContext();

    v8::Local<v8::Object> obj = MapToObject(this->result, context);
    v8::Local<v8::Value> argv[2] = {
      Nan::Null(),
      obj
    };

    callback->Call(2, argv, async_resource);
  }

  void HandleErrorCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::New<v8::String>(this->ErrorMessage()).ToLocalChecked(),
      Nan::Null()
    };

    callback->Call(2, argv, async_resource);
  }

  private:
    TagLib::String path;
    TagLib::Map<TagLib::String, TagLib::String> result;
};

class WriteTagsWorker : public Nan::AsyncWorker {
  public:
    WriteTagsWorker(Nan::Callback *callback, TagLib::String path, TagLib::PropertyMap map)
      : Nan::AsyncWorker(callback), path(path), map(map) {}
  ~WriteTagsWorker() { }

  void Execute() {
    TagLib::FileRef f(StringToFileName(path), false);
    if (f.isNull()) {
      this->SetErrorMessage("Could not parse file");
      return;
    }

    TagLib::PropertyMap existingProperties = ReadTags(f);
    TagLib::PropertyMap newProperties = MergePropertyMaps(existingProperties, this->map);
    WriteTags(f, newProperties);
  }

  void HandleOKCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::Null(),
      Nan::True()
    };

    callback->Call(2, argv, async_resource);
  }

  void HandleErrorCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::New<v8::String>(this->ErrorMessage()).ToLocalChecked(),
      Nan::Null()
    };

    callback->Call(2, argv, async_resource);
  }

  private:
    TagLib::String path;
    TagLib::PropertyMap map;
};

class WriteId3TagsWorker : public Nan::AsyncWorker {
  public:
    WriteId3TagsWorker(Nan::Callback *callback, TagLib::String path, TagLib::Map<TagLib::String, TagLib::String> map)
      : Nan::AsyncWorker(callback), path(path), map(map) {}
  ~WriteId3TagsWorker() { }

  void Execute() {
    TagLib::FileRef f(StringToFileName(path), false);
    if (f.isNull()) {
      this->SetErrorMessage("Could not parse file");
      return;
    }

    WriteId3Tags(f, this->map);
  }

  void HandleOKCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::Null(),
      Nan::True()
    };

    callback->Call(2, argv, async_resource);
  }

  void HandleErrorCallback() {
    v8::Local<v8::Value> argv[2] = {
      Nan::New<v8::String>(this->ErrorMessage()).ToLocalChecked(),
      Nan::Null()
    };

    callback->Call(2, argv, async_resource);
  }

  private:
    TagLib::String path;
    TagLib::Map<TagLib::String, TagLib::String> map;
};

NAN_METHOD(writeTags) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 3) {
    Nan::ThrowTypeError("Expected 3 arguments");
    return;
  }

  if (!ValidatePath(info[0])
      || !ValidateProperties(info[1])
      || !ValidateCallback(info[2])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Object> opt_props = info[1].As<v8::Object>();
  v8::Local<v8::Function> opt_callback = info[2].As<v8::Function>();

  TagLib::String path = StringToTagLibString(opt_path);
  TagLib::PropertyMap map = ObjectToPropertyMap(opt_props, context);

  Nan::Callback *callback = new Nan::Callback(opt_callback);
  AsyncQueueWorker(new WriteTagsWorker(callback, path, map));
}

NAN_METHOD(writeTagsSync) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!ValidatePath(info[0]) || !ValidateProperties(info[1])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Object> opt_props = info[1].As<v8::Object>();

  TagLib::String path = StringToTagLibString(opt_path);
  TagLib::FileRef f(StringToFileName(path), false);
  if (!ValidateFile(f)) {
    return;
  }

  TagLib::PropertyMap map = ObjectToPropertyMap(opt_props, context);
  TagLib::PropertyMap existingProperties = ReadTags(f);
  TagLib::PropertyMap newProperties = MergePropertyMaps(existingProperties, map);
  WriteTags(f, newProperties);

  info.GetReturnValue().Set(Nan::True());
}

NAN_METHOD(writeId3Tags) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 3) {
    Nan::ThrowTypeError("Expected 3 arguments");
    return;
  }

  if (!ValidatePath(info[0])
      || !ValidateProperties(info[1])
      || !ValidateCallback(info[2])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Object> opt_props = info[1].As<v8::Object>();
  v8::Local<v8::Function> opt_callback = info[2].As<v8::Function>();

  TagLib::String path = StringToTagLibString(opt_path);
  TagLib::Map<TagLib::String, TagLib::String> map = ObjectToMap(opt_props, context);

  Nan::Callback *callback = new Nan::Callback(opt_callback);
  AsyncQueueWorker(new WriteId3TagsWorker(callback, path, map));
}

NAN_METHOD(writeId3TagsSync) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!ValidatePath(info[0]) || !ValidateProperties(info[1])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Object> opt_props = info[1].As<v8::Object>();

  TagLib::String path = StringToTagLibString(opt_path);
  TagLib::FileRef f(StringToFileName(path), false);
  if (!ValidateFile(f)) {
    return;
  }

  TagLib::Map<TagLib::String, TagLib::String> map = ObjectToMap(opt_props, context);
  WriteId3Tags(f, map);

  info.GetReturnValue().Set(Nan::True());
}

NAN_METHOD(readTags) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!ValidatePath(info[0]) || !ValidateCallback(info[1])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Function> opt_callback = info[1].As<v8::Function>();

  TagLib::String path = StringToTagLibString(opt_path);

  Nan::Callback *callback = new Nan::Callback(opt_callback);
  AsyncQueueWorker(new ReadTagsWorker(callback, path));
}

NAN_METHOD(readTagsSync) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 1) {
    Nan::ThrowTypeError("Expected 1 argument");
    return;
  }

  if (!ValidatePath(info[0])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();

  TagLib::String path = StringToTagLibString(opt_path);

  TagLib::FileRef f(StringToFileName(path), false);
  if (!ValidateFile(f)) {
    return;
  }
  TagLib::PropertyMap map = ReadTags(f);

  v8::Local<v8::Object> obj = PropertyMapToObject(map, context);

  info.GetReturnValue().Set(obj);
}

NAN_METHOD(readAudioPropertiesSync) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 1) {
    Nan::ThrowTypeError("Expected 1 argument");
    return;
  }

  if (!ValidatePath(info[0])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();

  TagLib::String path = StringToTagLibString(opt_path);

  TagLib::FileRef f(StringToFileName(path), true, TagLib::AudioProperties::Fast);
  if (!ValidateFile(f)) {
    return;
  }
  TagLib::Map<TagLib::String, TagLib::String> map = ReadAudioProperties(f);

  v8::Local<v8::Object> obj = MapToObject(map, context);

  info.GetReturnValue().Set(obj);
}

NAN_METHOD(readAudioProperties) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!ValidatePath(info[0]) || !ValidateCallback(info[1])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Function> opt_callback = info[1].As<v8::Function>();

  TagLib::String path = StringToTagLibString(opt_path);

  Nan::Callback *callback = new Nan::Callback(opt_callback);
  AsyncQueueWorker(new ReadAudioPropertiesWorker(callback, path));
}

NAN_METHOD(readId3Tags) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!ValidatePath(info[0]) || !ValidateCallback(info[1])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();
  v8::Local<v8::Function> opt_callback = info[1].As<v8::Function>();

  TagLib::String path = StringToTagLibString(opt_path);

  Nan::Callback *callback = new Nan::Callback(opt_callback);
  AsyncQueueWorker(new ReadId3TagsWorker(callback, path));
}

NAN_METHOD(readId3TagsSync) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  if (info.Length() != 1) {
    Nan::ThrowTypeError("Expected 1 argument");
    return;
  }

  if (!ValidatePath(info[0])) {
    return;
  }

  v8::Local<v8::String> opt_path = info[0].As<v8::String>();

  TagLib::String path = StringToTagLibString(opt_path);

  TagLib::FileRef f(StringToFileName(path), false);
  if (!ValidateFile(f)) {
    return;
  }
  TagLib::Map<TagLib::String, TagLib::String> map = ReadId3Tags(f);

  v8::Local<v8::Object> obj = MapToObject(map, context);

  info.GetReturnValue().Set(obj);
}

void Init(v8::Local<v8::Object> exports, v8::Local<v8::Value> module, void *) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();
  exports->Set(context,
    Nan::New("writeTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(writeTagsSync)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("writeTags").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(writeTags)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("writeId3TagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(writeId3TagsSync)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("writeId3Tags").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(writeId3Tags)->GetFunction(context).ToLocalChecked()
  );

  exports->Set(context,
    Nan::New("readTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readTagsSync)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("readTags").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readTags)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("readId3TagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readId3TagsSync)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("readId3Tags").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readId3Tags)->GetFunction(context).ToLocalChecked()
  );

  exports->Set(context,
    Nan::New("readAudioPropertiesSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readAudioPropertiesSync)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("readAudioProperties").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readAudioProperties)->GetFunction(context).ToLocalChecked()
  );
}

NODE_MODULE(taglib3, Init)
