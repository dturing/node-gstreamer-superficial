#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <v8.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <string.h>
#include <stdlib.h>

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv );

v8::Handle<v8::Object> createBuffer(const char *data, int length) {
	NanEscapableScope();

	v8::Local<v8::Object> buffer = NanNewBufferHandle(data, length);

	return NanEscapeScope(buffer);
}

v8::Handle<v8::Value> gstsample_to_v8( GstSample *sample ) {
	GstMapInfo map;
	GstBuffer *buf = gst_sample_get_buffer( sample );
	if( gst_buffer_map( buf, &map, GST_MAP_READ ) ) {
		const unsigned char *data = map.data;
		int length = map.size;
		v8::Handle<v8::Object> frame = createBuffer( (const char *)data, length );
		return frame;
	}

	return NanUndefined();
}

v8::Handle<v8::Value> gstvaluearray_to_v8( const GValue *gv ) {
	if( !GST_VALUE_HOLDS_ARRAY(gv) ) {
		NanThrowTypeError("not a GstValueArray");
		return NanUndefined();
	}

	int size = gst_value_array_get_size(gv);
	v8::Handle<v8::Array> array = NanNew<v8::Array>(gst_value_array_get_size(gv));

	for( int i=0; i<size; i++ ) {
		array->Set( NanNew<v8::Number>(i), gvalue_to_v8(gst_value_array_get_value(gv,i)) );
	}
	return array;
}

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv ) {
	switch( G_VALUE_TYPE(gv) ) {
		case G_TYPE_STRING:
			return NanNew<v8::String>( g_value_get_string(gv) );
		case G_TYPE_BOOLEAN:
			return NanNew<v8::Boolean>( g_value_get_boolean(gv) );
		case G_TYPE_INT:
			return NanNew<v8::Number>( g_value_get_int(gv) );
		case G_TYPE_UINT:
			return NanNew<v8::Number>( g_value_get_uint(gv) );
		case G_TYPE_FLOAT:
			return NanNew<v8::Number>( g_value_get_float(gv) );
		case G_TYPE_DOUBLE:
			return NanNew<v8::Number>( g_value_get_double(gv) );
	}

	if( GST_VALUE_HOLDS_ARRAY(gv) ) {
		return gstvaluearray_to_v8( gv );
		/* FIXME
    } else if( GST_VALUE_HOLDS_BUFFER(gv) ) {
        GstBuffer *buf = gst_value_get_buffer(gv);
        if( buf == NULL ) {
            return v8::Undefined();
        }
        return gstbuffer_to_v8( buf );
        */
	}

	GValue b = G_VALUE_INIT;
	/* Attempt to transform it into a GValue of type STRING */
	g_value_init (&b, G_TYPE_STRING);
	if( !g_value_type_transformable (G_TYPE_INT, G_TYPE_STRING) ) {
		return NanUndefined();
	}

//	printf("Value is of unhandled type %s\n", g_type_name( G_VALUE_TYPE(gv) ) );

	g_value_transform( gv, &b );

	const char *str = g_value_get_string( &b );
	if( str == NULL ) return NanUndefined();
	return NanNew<v8::String>( g_value_get_string (&b) );
}

void v8_to_gvalue( v8::Handle<v8::Value> v, GValue *gv ) {
	if( v->IsNumber() ) {
        g_value_init( gv, G_TYPE_FLOAT );
        g_value_set_float( gv, v->NumberValue() );
	} else if( v->IsString() ) {
		v8::String::Utf8Value value( v->ToString() );
        g_value_init( gv, G_TYPE_STRING );
        g_value_set_string( gv, *value );
	} else if( v->IsBoolean() ) {
        g_value_init( gv, G_TYPE_BOOLEAN );
        g_value_set_boolean( gv, v->BooleanValue() );
	}

	return;
}


/* --------------------------------------------------
    GstStructure helpers
   -------------------------------------------------- */
gboolean gst_structure_to_v8_value_iterate( GQuark field_id, const GValue *val, gpointer user_data ) {
	v8::Handle<v8::Object> *obj = (v8::Handle<v8::Object>*)user_data;
	v8::Handle<v8::Value> v = gvalue_to_v8( val );
	(*obj)->Set(NanNew<v8::String>((const char *)g_quark_to_string(field_id)), v );
	return true;
}

v8::Handle<v8::Object> gst_structure_to_v8( v8::Handle<v8::Object> obj, GstStructure *struc ) {
	const gchar *name = gst_structure_get_name(struc);
	obj->Set(NanNew<v8::String>("name"), NanNew<v8::String>( name ) );
	gst_structure_foreach( struc, gst_structure_to_v8_value_iterate, &obj );
	return obj;
}



class GObjectWrap : public node::ObjectWrap {
	public:
		static void Init();
		static v8::Handle<v8::Value> NewInstance( const v8::FunctionCallbackInfo<v8::Value>& args, GObject *obj );

		void set( const char *name, const v8::Handle<v8::Value> value );

		void play();
		void pause();
		void stop();
		
	private:
		GObjectWrap();
		~GObjectWrap();
		
		GObject *obj;
		
		static v8::Persistent<v8::Function> constructor;
		static NAN_METHOD(New);
		static NAN_METHOD(_get);
		static NAN_METHOD(_set);
		static NAN_METHOD(_play);
		static NAN_METHOD(_pause);
		static NAN_METHOD(_stop);

//		static v8::Handle<v8::Value> _onBufferAvailable(const v8::Arguments& args);
		static void _doPullBuffer( uv_work_t *req );
		static void _pulledBuffer( uv_work_t *req, int );
		static NAN_METHOD(_pull);
};

v8::Persistent<v8::Function> GObjectWrap::constructor;

GObjectWrap::GObjectWrap() {
}

GObjectWrap::~GObjectWrap() {
}

void GObjectWrap::Init() {
	v8::Local<v8::FunctionTemplate> tpl = NanNew<v8::FunctionTemplate>(New);
	tpl->SetClassName(NanNew<v8::String>("GObjectWrap"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("get"),
			NanNew<v8::FunctionTemplate>(_get)->GetFunction());
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("set"),
			NanNew<v8::FunctionTemplate>(_set)->GetFunction());

	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("pull"),
			NanNew<v8::FunctionTemplate>(_pull)->GetFunction());

	NanAssignPersistent(constructor, tpl->GetFunction());
}

NAN_METHOD(GObjectWrap::New) {
  	GObjectWrap* obj = new GObjectWrap();
	obj->Wrap(args.This());

	NanReturnValue(args.This());
}

v8::Handle<v8::Value> GObjectWrap::NewInstance( const v8::FunctionCallbackInfo<v8::Value>& args, GObject *obj ) {
	NanEscapableScope();
	const unsigned argc = 1;
	v8::Local<v8::Function> constructorLocal = NanNew(constructor);
	v8::Handle<v8::Value> argv[argc] = { args[0] };
	v8::Local<v8::Object> instance = constructorLocal->NewInstance(argc, argv);

	GObjectWrap* wrap = ObjectWrap::Unwrap<GObjectWrap>(instance);
  	wrap->obj = obj;

  	NanEscapeScope(instance);
}

NAN_METHOD(GObjectWrap::_get) {
	NanEscapableScope();
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());

	v8::Local<v8::String> nameString = args[0]->ToString();
	v8::String::Utf8Value name( nameString );
	
	GObject *o = obj->obj;
    GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(o), *name );
    if( !spec ) {
		NanThrowError(v8::String::Concat(
				NanNew<v8::String>("No such GObject property: "),
				nameString
		));
		NanEscapeScope(NanUndefined());
		return;
    }

    GValue gv;
    memset( &gv, 0, sizeof( gv ) );
    g_value_init( &gv, G_PARAM_SPEC_VALUE_TYPE(spec) );
    g_object_get_property( o, *name, &gv );
        
    NanEscapeScope( gvalue_to_v8( &gv ) );
}

void GObjectWrap::set( const char *name, const v8::Handle<v8::Value> value ) {
	GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(obj), name );
	if( !spec ) {
		NanThrowError(v8::String::Concat(
				NanNew<v8::String>("No such GObject property: "),
				NanNew<v8::String>(name)
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
	NanEscapableScope();
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());

	if( args[0]->IsString() ) {
		v8::String::Utf8Value name( args[0]->ToString() );
		obj->set( *name, args[1] );
		
	} else if( args[0]->IsObject() ) {
		v8::Local<v8::Object> o = args[0]->ToObject();
		
		const v8::Local<v8::Array> props = o->GetPropertyNames();
		const int length = props->Length();
		for( int i=0; i<length; i++ ) {
			const v8::Local<v8::Value> key = props->Get(i);
			v8::String::Utf8Value name( key->ToString() );
			const v8::Local<v8::Value> value = o->Get(key);
			obj->set( *name, value );
		}		
	
	} else {
		NanThrowTypeError("set expects name,value or object");
		NanEscapeScope(NanUndefined());
		return;
	}
	NanEscapeScope( args[1] );
}

NAN_METHOD(GObjectWrap::_play) {
	NanEscapableScope();
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());
	obj->play();
	NanEscapeScope(NanTrue());
}
NAN_METHOD(GObjectWrap::_pause) {
	NanEscapableScope();
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());
	obj->pause();
	NanEscapeScope(NanTrue());
}
NAN_METHOD(GObjectWrap::_stop) {
	NanEscapableScope();
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());
	obj->stop();
	NanEscapeScope(NanTrue());
}

struct SampleRequest {
	uv_work_t request;
	v8::Persistent<v8::Function> cb_buffer, cb_caps;
	GstSample *sample;
	GstCaps *caps;
	GObjectWrap *obj;
};

void GObjectWrap::_doPullBuffer( uv_work_t *req ) {
	SampleRequest *br = static_cast<SampleRequest*>(req->data);

	GstAppSink *sink = GST_APP_SINK(br->obj->obj);
	br->sample = gst_app_sink_pull_sample( sink );
}

void GObjectWrap::_pulledBuffer( uv_work_t *req, int n ) {
	SampleRequest *br = static_cast<SampleRequest*>(req->data);
	
	if( br->sample ) {
		NanEscapableScope();
/* FIXME
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
		v8::Handle<v8::Value> buf = gstsample_to_v8( br->sample );
		
		v8::Handle<v8::Value> argv[1] = { buf };

		v8::Local<v8::Function> cbBufferLocal = NanNew(br->cb_buffer);

		cbBufferLocal->Call(NanGetCurrentContext()->Global(), 1, argv);

		gst_sample_unref(br->sample);
		br->sample = NULL;
		
		NanEscapeScope(NanUndefined());
	}

	uv_queue_work( uv_default_loop(), &br->request, _doPullBuffer, _pulledBuffer );
}

NAN_METHOD(GObjectWrap::_pull) {
	NanEscapableScope();
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());

	if( args.Length() < 2 || !args[0]->IsFunction() || !args[1]->IsFunction() ) {
		NanThrowError("Callbacks are required and must be Functions.");
		NanEscapeScope(NanUndefined());
		return;
	}
  
    if( !GST_IS_APP_SINK( obj->obj ) ) {
		NanThrowError("not a GstAppSink");
		NanEscapeScope(NanUndefined());
		return;
    }

	v8::Handle<v8::Function> cb_buffer = v8::Handle<v8::Function>::Cast(args[0]);
	v8::Handle<v8::Function> cb_caps = v8::Handle<v8::Function>::Cast(args[1]);

	SampleRequest * br = new SampleRequest();
	br->request.data = br;
	NanAssignPersistent(br->cb_buffer, cb_buffer);
	NanAssignPersistent(br->cb_caps, cb_caps);
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work( uv_default_loop(), &br->request, _doPullBuffer, _pulledBuffer );

	NanEscapeScope(NanUndefined());
}


class Pipeline : public node::ObjectWrap {
	public:
		static void Init( v8::Handle<v8::Object> exports );
		
		void play();
		void pause();
		void stop();
		void forceKeyUnit(GObject* sink, int cnt);
		
		GObject *findChild( const char *name );
		v8::Handle<v8::Value> pollBus();
		
	private:
		Pipeline( const char *launch );
		~Pipeline();
		
		GstBin *pipeline;
		
		static NAN_METHOD(New);
		static NAN_METHOD(_play);
		static NAN_METHOD(_pause);
		static NAN_METHOD(_stop);
		static NAN_METHOD(_forceKeyUnit);
		static NAN_METHOD(_findChild);

		static void _doPollBus( uv_work_t *req );
		static void _polledBus( uv_work_t *req, int );
		static NAN_METHOD(_pollBus);
		
};

Pipeline::Pipeline( const char *launch ) {
	GError *err = NULL;

	pipeline = GST_BIN(gst_parse_launch(launch, &err));
	if( err ) {
		fprintf(stderr,"GstError: %s\n", err->message );
		NanThrowError(err->message);
	}
}

Pipeline::~Pipeline() {
}

void Pipeline::play() {
    gst_element_set_state( GST_ELEMENT(pipeline), GST_STATE_PLAYING );
}

void Pipeline::stop() {
    gst_element_set_state( GST_ELEMENT(pipeline), GST_STATE_NULL );
}

void Pipeline::pause() {
    gst_element_set_state( GST_ELEMENT(pipeline), GST_STATE_PAUSED );
}

void Pipeline::forceKeyUnit(GObject *sink, int cnt) {
    GstPad *sinkpad = gst_element_get_static_pad (GST_ELEMENT(sink), "sink");
    printf("force key unit %d\n", cnt);
    gst_pad_push_event (sinkpad, (GstEvent*) gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, cnt));
}

GObject * Pipeline::findChild( const char *name ) {
    GstElement *e = gst_bin_get_by_name( GST_BIN(pipeline), name );
    return G_OBJECT(e);
}

struct BusRequest {
	uv_work_t request;
	v8::Persistent<v8::Function> callback;
	Pipeline *obj;
	GstMessage *msg;
};

void Pipeline::_doPollBus( uv_work_t *req ) {
	BusRequest *br = static_cast<BusRequest*>(req->data);
	GstBus *bus = gst_element_get_bus( GST_ELEMENT(br->obj->pipeline));
	if( !bus ) return;
	br->msg = gst_bus_timed_pop( bus, GST_CLOCK_TIME_NONE );
}

void Pipeline::_polledBus( uv_work_t *req, int n ) {
	BusRequest *br = static_cast<BusRequest*>(req->data);


	if( br->msg ) {
//		printf("Got Bus Message type %s\n", GST_MESSAGE_TYPE_NAME(msg) );
		v8::Local<v8::Object> m = NanNew<v8::Object>();
		m->Set(NanNew<v8::String>("type"), NanNew<v8::String>(GST_MESSAGE_TYPE_NAME(br->msg)) );
/*	
        if( br->msg->structure ) {
            gst_structure_to_v8( m, br->msg->structure );
        }
*/
		if( GST_MESSAGE_TYPE(br->msg) == GST_MESSAGE_ERROR ) {
			GError *err = NULL;
			gchar *name;
			name = gst_object_get_path_string (br->msg->src);
			m->Set(NanNew<v8::String>("name"), NanNew<v8::String>( name ) );
			gst_message_parse_error (br->msg, &err, NULL);
			m->Set(NanNew<v8::String>("message"), NanNew<v8::String>( err->message) );
		}

		v8::Local<v8::Value> argv[1] = { m };
		v8::Local<v8::Function> cbCallbackLocal = NanNew(br->callback);
		cbCallbackLocal->Call(NanGetCurrentContext()->Global(), 1, argv);
		
		gst_message_unref( br->msg );
	}
		
	uv_queue_work( uv_default_loop(), &br->request, _doPollBus, _polledBus );
}

NAN_METHOD(Pipeline::_pollBus) {
	NanEscapableScope();
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());

	if( args.Length() == 0 || !args[0]->IsFunction() ) {
		NanThrowError("Callback is required and must be a Function.");
		NanEscapeScope(NanUndefined());
		return;
	}
  

	v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(args[0]);
	
	BusRequest * br = new BusRequest();
	br->request.data = br;
	NanAssignPersistent(br->callback, callback);
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work( uv_default_loop(), &br->request, _doPollBus, _polledBus );
	NanEscapeScope(NanUndefined());
}



void Pipeline::Init( v8::Handle<v8::Object> exports ) {
	v8::Local<v8::FunctionTemplate> tpl = NanNew<v8::FunctionTemplate>(New);
	tpl->SetClassName(NanNew<v8::String>("Pipeline"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("play"),
			NanNew<v8::FunctionTemplate>(_play)->GetFunction());
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("pause"),
			NanNew<v8::FunctionTemplate>(_pause)->GetFunction());
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("stop"),
			NanNew<v8::FunctionTemplate>(_stop)->GetFunction());
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("forceKeyUnit"),
			NanNew<v8::FunctionTemplate>(_forceKeyUnit)->GetFunction());
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("findChild"),
			NanNew<v8::FunctionTemplate>(_findChild)->GetFunction());
	tpl->PrototypeTemplate()->Set(NanNew<v8::String>("pollBus"),
			NanNew<v8::FunctionTemplate>(_pollBus)->GetFunction());

	v8::Persistent<v8::Function> constructorPersist;
	NanAssignPersistent(constructorPersist, tpl->GetFunction());

	v8::Local<v8::Function> constructor = NanNew(constructorPersist);

	exports->Set(NanNew<v8::String>("Pipeline"), constructor);
}

NAN_METHOD(Pipeline::New) {
	NanEscapableScope();

	v8::String::Utf8Value launch( args[0]->ToString() );

	Pipeline* obj = new Pipeline( *launch );
	obj->Wrap(args.This());

	NanEscapeScope(args.This());
}

NAN_METHOD(Pipeline::_play) {
	NanEscapableScope();
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	obj->play();
	NanEscapeScope(NanTrue());
}
NAN_METHOD(Pipeline::_pause) {
	NanEscapableScope();
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	obj->pause();
	NanEscapeScope(NanTrue());
}
NAN_METHOD(Pipeline::_stop) {
	NanEscapableScope();
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	obj->stop();
	NanEscapeScope(NanTrue());
}
NAN_METHOD(Pipeline::_forceKeyUnit) {
	NanEscapableScope();
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	v8::String::Utf8Value name( args[0]->ToString() );
	GObject *o = obj->findChild( *name );
	int cnt( args[1]->Int32Value() );
	obj->forceKeyUnit( o, cnt );
	NanEscapeScope(NanTrue());
}
NAN_METHOD(Pipeline::_findChild) {
	NanEscapableScope();
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());

	v8::String::Utf8Value name( args[0]->ToString() );
	GObject *o = obj->findChild( *name );
	if ( o ) {
		NanEscapeScope( GObjectWrap::NewInstance( args, o ) );
	} else {
		NanEscapeScope(NanUndefined());
	}
}

void init( v8::Handle<v8::Object> exports ) {
	gst_init( NULL, NULL );
	GObjectWrap::Init();
	Pipeline::Init(exports);
}

NODE_MODULE(gstreamer_superficial, init);
