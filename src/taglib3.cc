#include <v8.h>
#include <nan.h>
#include <node.h>
#include <node_buffer.h>

#define NDEBUG
#define TAGLIB_STATIC
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/tpropertymap.h>

// TagLib string -> V8 string
v8::Local<v8::String> TagLibStringToString(TagLib::String s) {
  return Nan::New<v8::String>(s.toCString(true)).ToLocalChecked();
}

// V8 string -> TagLib string
TagLib::String StringToTagLibString(v8::Local<v8::String> s) {
  return TagLib::String(*Nan::Utf8String(s), TagLib::String::UTF8);
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

// property map -> v8 object
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

// merge v8 object into property map
TagLib::PropertyMap ObjectToPropertyMap(v8::Local<v8::Object> props, v8::Local<v8::Context> context) {
  v8::Local<v8::Array> property_names = props->GetOwnPropertyNames(context).ToLocalChecked();
  TagLib::PropertyMap *map = new TagLib::PropertyMap();

  for (int i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::String> key = property_names->Get(context, i).ToLocalChecked().As<v8::String>();

    TagLib::StringList list = TagLib::StringList();

    v8::Local<v8::Array> values = props->Get(context, key).ToLocalChecked().As<v8::Array>();
    for (int j = 0; j < values->Length(); ++j) {
      v8::Local<v8::String> value = values->Get(context, j).ToLocalChecked().As<v8::String>();
      list.append(StringToTagLibString(value));
    }

    map->insert(StringToTagLibString(key), list);
  }

  return *map;
}

// merge two property maps, replacing existing map1 entries with the corresponding map1 entry
TagLib::PropertyMap MergePropertyMaps(TagLib::PropertyMap map1, TagLib::PropertyMap map2) {
  TagLib::PropertyMap *map = new TagLib::PropertyMap();

  for (TagLib::PropertyMap::ConstIterator i = map1.begin(); i != map1.end(); ++i) {
    map->replace(i->first, i->second);
  }
  for (TagLib::PropertyMap::ConstIterator i = map2.begin(); i != map2.end(); ++i) {
    map->replace(i->first, i->second);
  }

  return *map;
}

TagLib::PropertyMap ReadTags(TagLib::FileRef f) {
  return f.file()->properties();
}

void WriteTags(TagLib::FileRef f, TagLib::PropertyMap map) {
  if (map.size() > 0) {
    f.file()->setProperties(map);
    f.file()->save();
  }
}

class ReadTagsWorker : public Nan::AsyncWorker {
  public:
    ReadTagsWorker(Nan::Callback *callback, TagLib::String path)
      : Nan::AsyncWorker(callback), path(path) {}
  ~ReadTagsWorker() { }

  void Execute() {
    TagLib::FileRef f(path.toCString(), false);
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

class WriteTagsWorker : public Nan::AsyncWorker {
  public:
    WriteTagsWorker(Nan::Callback *callback, TagLib::String path, TagLib::PropertyMap map)
      : Nan::AsyncWorker(callback), path(path), map(map) {}
  ~WriteTagsWorker() { }

  void Execute() {
    TagLib::FileRef f(path.toCString(), false);
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
  TagLib::FileRef f(path.toCString(), false);
  if (!ValidateFile(f)) {
    return;
  }

  TagLib::PropertyMap map = ObjectToPropertyMap(opt_props, context);
  TagLib::PropertyMap existingProperties = ReadTags(f);
  TagLib::PropertyMap newProperties = MergePropertyMaps(existingProperties, map);
  WriteTags(f, newProperties);

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

  TagLib::FileRef f(path.toCString(), false);
  if (!ValidateFile(f)) {
    return;
  }
  TagLib::PropertyMap map = f.file()->properties();

  v8::Local<v8::Object> obj = PropertyMapToObject(map, context);

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
    Nan::New("readTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readTagsSync)->GetFunction(context).ToLocalChecked()
  );
  exports->Set(context,
    Nan::New("readTags").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readTags)->GetFunction(context).ToLocalChecked()
  );
}

NODE_MODULE(taglib3, Init)
