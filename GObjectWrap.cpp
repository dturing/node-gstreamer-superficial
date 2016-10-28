#include <nan.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "GObjectWrap.h"

Nan::Persistent<v8::Function> GObjectWrap::constructor;

GObjectWrap::GObjectWrap() {
}

GObjectWrap::~GObjectWrap() {
}

void GObjectWrap::Init() {
	v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
	tpl->SetClassName(Nan::New<v8::String>("GObjectWrap").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("get").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_get));
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("set").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_set));

	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("pull").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_pull));

	constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GObjectWrap::New) {
  	GObjectWrap* obj = new GObjectWrap();
	obj->Wrap(info.This());

	info.GetReturnValue().Set(info.This());
}

v8::Handle<v8::Value> GObjectWrap::NewInstance( const Nan::FunctionCallbackInfo<v8::Value>& info, GObject *obj ) {
	Nan::EscapableHandleScope scope;
	const unsigned argc = 1;
	v8::Isolate* isolate = info.GetIsolate();

	v8::Local<v8::Context> context = isolate->GetCurrentContext();
	v8::Local<v8::Function> constructorLocal = Nan::New(constructor);
	v8::Handle<v8::Value> argv[argc] = { info[0] };
	v8::Local<v8::Object> instance = constructorLocal->NewInstance(context, argc, argv).ToLocalChecked();

	GObjectWrap* wrap = Nan::ObjectWrap::Unwrap<GObjectWrap>(instance);
  	wrap->obj = obj;
	
  	info.GetReturnValue().Set(instance);
  	return scope.Escape(instance);
}

NAN_METHOD(GObjectWrap::_get) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

	v8::Local<v8::String> nameString = info[0]->ToString();
	v8::String::Utf8Value name( nameString );
	
	GObject *o = obj->obj;
    GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(o), *name );
    if( !spec ) {
		Nan::ThrowError(v8::String::Concat(
				Nan::New<v8::String>("No such GObject property: ").ToLocalChecked(),
				nameString
		));
		scope.Escape(Nan::Undefined());
		return;
    }

    GValue gv;
    memset( &gv, 0, sizeof( gv ) );
    g_value_init( &gv, G_PARAM_SPEC_VALUE_TYPE(spec) );
    g_object_get_property( o, *name, &gv );
        
    scope.Escape( gvalue_to_v8( &gv ) );
}

void GObjectWrap::set( const char *name, const v8::Handle<v8::Value> value ) {
	GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(obj), name );
	if( !spec ) {
		Nan::ThrowError(v8::String::Concat(
				Nan::New<v8::String>("No such GObject property: ").ToLocalChecked(),
				Nan::New<v8::String>(name).ToLocalChecked()
		));
		return;
	}

	GValue gv;
	memset( &gv, 0, sizeof( gv ) );
	v8_to_gvalue( value, &gv );
	g_object_set_property( obj, name, &gv );
}

void GObjectWrap::play() {
    gst_element_set_state( GST_ELEMENT(obj), GST_STATE_PLAYING );
}

void GObjectWrap::stop() {
    gst_element_set_state( GST_ELEMENT(obj), GST_STATE_NULL );
}

void GObjectWrap::pause() {
    gst_element_set_state( GST_ELEMENT(obj), GST_STATE_PAUSED );
}


NAN_METHOD(GObjectWrap::_set) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

	if( info[0]->IsString() ) {
		v8::String::Utf8Value name( info[0]->ToString() );
		obj->set( *name, info[1] );
		
	} else if( info[0]->IsObject() ) {
		v8::Local<v8::Object> o = info[0]->ToObject();
		
		const v8::Local<v8::Array> props = o->GetPropertyNames();
		const int length = props->Length();
		for( int i=0; i<length; i++ ) {
			const v8::Local<v8::Value> key = props->Get(i);
			v8::String::Utf8Value name( key->ToString() );
			const v8::Local<v8::Value> value = o->Get(key);
			obj->set( *name, value );
		}		
	
	} else {
		Nan::ThrowTypeError("set expects name,value or object");
		scope.Escape(Nan::Undefined());
		return;
	}
	scope.Escape( info[1] );
}

NAN_METHOD(GObjectWrap::_play) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	obj->play();
	scope.Escape(Nan::True());
}
NAN_METHOD(GObjectWrap::_pause) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	obj->pause();
	scope.Escape(Nan::True());
}
NAN_METHOD(GObjectWrap::_stop) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	obj->stop();
	scope.Escape(Nan::True());
}

class PullWorker : public Nan::AsyncWorker {
	public:
		PullWorker(Nan::Callback *callback, GstAppSink *appsink)
		: AsyncWorker(callback), appsink(appsink) {};

		~PullWorker() {}

		void Execute() {
			sample = gst_app_sink_pull_sample(appsink);
		}

		void HandleOKCallback() {
			Nan::HandleScope scope;

			v8::Local<v8::Value> buf;
			if (sample) {
				buf = gstsample_to_v8( sample );
				gst_sample_unref(sample);
				sample = NULL;
			} else {
				buf =  Nan::Null();
			}

			v8::Local<v8::Value> argv[] = { buf };
			callback->Call(1, argv);
		}

	private:
		GstAppSink *appsink;
		GstSample *sample;
};

NAN_METHOD(GObjectWrap::_pull) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

    if( !GST_IS_APP_SINK( obj->obj ) ) {
		Nan::ThrowError("not a GstAppSink");
		scope.Escape(Nan::Undefined());
		return;
    }

    Nan::Callback *callback = new Nan::Callback(info[0].As<v8::Function>());

    AsyncQueueWorker(new PullWorker(callback, GST_APP_SINK(obj->obj)));
}

/* FIXME, maybe, if we want to transfer caps...
		GstCaps *caps = gst_buffer_get_caps (br->buffer); 
		if( caps && !gst_caps_is_equal( caps, br->caps )) {
			br->caps = caps;

			GstStructure *structure = gst_caps_get_structure (caps, 0);
			
			v8::Local<v8::Object> _caps = v8::Object::New();
		    if( structure ) {
		        gst_structure_to_v8( _caps, structure );
		    }
			
			v8::Local<v8::Value> argv[1] = { _caps };
			br->cb_caps->Call(v8::Context::GetCurrent()->Global(), 1, argv);
		}
*/
