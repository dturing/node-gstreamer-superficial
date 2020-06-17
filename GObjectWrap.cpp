#include <nan.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include "GObjectWrap.h"

Nan::Persistent<Function> GObjectWrap::constructor;

void GObjectWrap::Init() {
	Nan::HandleScope scope;

	// Constructor
	Local<FunctionTemplate> ctor = Nan::New<FunctionTemplate>(GObjectWrap::New);
	ctor->InstanceTemplate()->SetInternalFieldCount(1);
	ctor->SetClassName(Nan::New<String>("GObjectWrap").ToLocalChecked());
	// Prototype
	//Local<ObjectPrototype> proto = ctor->PrototypeTemplate();
	constructor.Reset(Nan::GetFunction(ctor).ToLocalChecked());
}

NAN_METHOD(GObjectWrap::New) {
	GObjectWrap* obj = new GObjectWrap();
	obj->Wrap(info.This());

	info.GetReturnValue().Set(info.This());
}

Local<Value> GObjectWrap::NewInstance( const Nan::FunctionCallbackInfo<Value>& info, GObject *obj ) {
	Nan::EscapableHandleScope scope;
	const unsigned argc = 1;
	Isolate* isolate = info.GetIsolate();

	Local<Context> context = isolate->GetCurrentContext();
	Local<Function> constructorLocal = Nan::New(constructor);
	constructorLocal->SetName(String::NewFromUtf8(isolate, G_OBJECT_TYPE_NAME(obj), v8::NewStringType::kNormal).ToLocalChecked());
	Local<Value> argv[argc] = { info[0] };
	Local<Object> instance = constructorLocal->NewInstance(context, argc, argv).ToLocalChecked();

	GObjectWrap* wrap = Nan::ObjectWrap::Unwrap<GObjectWrap>(instance);
	wrap->obj = obj;
	guint n_properties;
	GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(obj), &n_properties);
	for(guint i = 0; i < n_properties; i++) {
		GParamSpec *property = properties[i];
		Local<String> name = String::NewFromUtf8(isolate, property->name, v8::NewStringType::kNormal).ToLocalChecked();
		Nan::SetAccessor(instance, name, GObjectWrap::GetProperty, GObjectWrap::SetProperty);
	}

	if(GST_IS_APP_SINK(obj)) {
        Nan::SetMethod(instance, "pull", GstAppSinkPull);
	}
	if (GST_IS_APP_SRC(obj)) {
	    Nan::SetMethod(instance, "push", GstAppSrcPush);
	}

	info.GetReturnValue().Set(instance);
	return scope.Escape(instance);
}

NAN_GETTER(GObjectWrap::GetProperty) {
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	Nan::Utf8String name(property);

	GObject *o = obj->obj;
	GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(o), *name);

	if(!spec) {
		info.GetReturnValue().Set(Nan::Undefined());
	} else {
		GValue gv;
		memset(&gv, 0, sizeof(gv));
		g_value_init(&gv, G_PARAM_SPEC_VALUE_TYPE(spec));
		g_object_get_property(o, *name, &gv);

		info.GetReturnValue().Set(gvalue_to_v8(&gv));
	}
}

NAN_SETTER(GObjectWrap::SetProperty) {
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	Nan::Utf8String name(property);

	GObject *o = obj->obj;
	GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(o), *name);
	if(spec) {
		GValue gv;
		memset( &gv, 0, sizeof( gv ) );
		v8_to_gvalue( value, &gv, spec);
		g_object_set_property( o, *name, &gv );
	}
}

class PullWorker : public Nan::AsyncWorker {
	public:
		PullWorker(Nan::Callback *callback, GstAppSink *appsink)
		: AsyncWorker(callback,"node-gst-superficial.PullWorker"), appsink(appsink) {};

		~PullWorker() {}

		void Execute() {
			sample = gst_app_sink_pull_sample(appsink);
		}

		void HandleOKCallback() {
			Nan::HandleScope scope;

			Local<Value> buf;
			Local<Object> caps = Nan::New<Object>();
			if (sample) {

				GstCaps *gcaps = gst_sample_get_caps(sample);
				if (gcaps) {
					const GstStructure *structure = gst_caps_get_structure(gcaps,0);
					if (structure) gst_structure_to_v8(caps, structure);
				}

				buf = gstsample_to_v8(sample);
				gst_sample_unref(sample);
				sample = NULL;
			} else {
				buf =  Nan::Null();
			}

			Local<Value> argv[] = { buf, caps };
			callback->Call(2, argv, async_resource);
		}

	private:
		GstAppSink *appsink;
		GstSample *sample;
};

NAN_METHOD(GObjectWrap::GstAppSinkPull) {
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
	AsyncQueueWorker(new PullWorker(callback, GST_APP_SINK(obj->obj)));
}

NAN_METHOD(GObjectWrap::GstAppSrcPush) {
    auto *obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

    if (info.Length() > 0) {
        if (info[0]->IsObject()) {
        	Local<Object> o = Nan::To<Object>(info[0]).ToLocalChecked();
            char *buffer = node::Buffer::Data(o);
            size_t buffer_length = node::Buffer::Length(o);

            GstBuffer *gst_buffer = gst_buffer_new_allocate(nullptr, buffer_length, nullptr);
            gst_buffer_fill(gst_buffer, 0, buffer, buffer_length);

            if (info.Length() > 1) {
              char *bufferPTS = node::Buffer::Data(Nan::To<Object>(info[1]).ToLocalChecked());
              gst_buffer->pts = GST_READ_UINT64_BE(bufferPTS);
            }

            gst_app_src_push_buffer(GST_APP_SRC(obj->obj), gst_buffer);
           // gst_buffer_unref(gst_buffer);
        }
        // TODO throw an error if arg is not a buffer object?
    }
    // TODO throw an error if no args are given?
}
