#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string.h>

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv );

v8::Persistent<v8::Value> gstbuffer_to_v8( GstBuffer *buf ) {
	const char *data = (const char *)GST_BUFFER_DATA(buf);
	int length = GST_BUFFER_SIZE(buf);
	node::Buffer *slowBuffer = node::Buffer::New(length);
	memcpy(node::Buffer::Data(slowBuffer), data, length);
	return slowBuffer->handle_;
}

v8::Handle<v8::Value> gstvaluearray_to_v8( const GValue *gv ) {
	if( !GST_VALUE_HOLDS_ARRAY(gv) ) {
		return v8::ThrowException( v8::Exception::TypeError(v8::String::New("not a GstValueArray")) );
	}

	int size = gst_value_array_get_size(gv);
	v8::Handle<v8::Array> array = v8::Array::New(gst_value_array_get_size(gv));
	
	for( int i=0; i<size; i++ ) {
		array->Set( v8::Number::New(i), gvalue_to_v8(gst_value_array_get_value(gv,i)) );
	}
	return array;
/*
	first_element = gst_value_array_get_value (streamheader, 0);

	if (!GST_VALUE_HOLDS_BUFFER (first_element)) {
		return scope.Close(v8::ThrowException( v8::String::New("first streamheader not a buffer") ));
	}

	buf = gst_value_get_buffer (first_element);
	if (buf == NULL || GST_BUFFER_SIZE (buf) == 0) {
		return scope.Close(v8::ThrowException( v8::String::New("invalid first streamheader buffer") ));
	}
*/
}

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv ) {
    switch( G_VALUE_TYPE(gv) ) {
        case G_TYPE_STRING:
            return v8::String::New( g_value_get_string(gv) );
        case G_TYPE_BOOLEAN:
        	return v8::Boolean::New( g_value_get_boolean(gv) );
        case G_TYPE_INT:
            return v8::Number::New( g_value_get_int(gv) );
        case G_TYPE_UINT:
            return v8::Number::New( g_value_get_uint(gv) );
        case G_TYPE_FLOAT:
            return v8::Number::New( g_value_get_float(gv) );
        case G_TYPE_DOUBLE:
            return v8::Number::New( g_value_get_double(gv) );
    }

	if( GST_VALUE_HOLDS_ARRAY(gv) ) {
	   	return gstvaluearray_to_v8( gv );
	} else if( GST_VALUE_HOLDS_BUFFER(gv) ) {
		GstBuffer *buf = gst_value_get_buffer(gv);
		if( buf == NULL || GST_BUFFER_SIZE(buf) == 0 ) {
			return v8::Undefined();
		}
		return gstbuffer_to_v8( buf );
	}
    
    GValue b = G_VALUE_INIT;
	/* Attempt to transform it into a GValue of type STRING */
	g_value_init (&b, G_TYPE_STRING);
	if( !g_value_type_transformable (G_TYPE_INT, G_TYPE_STRING) ) {
		return v8::Undefined();
	}

//	printf("Value is of unhandled type %s\n", g_type_name( G_VALUE_TYPE(gv) ) );

	g_value_transform( gv, &b );

	const char *str = g_value_get_string( &b );
	if( str == NULL ) return v8::Undefined(); 
    return v8::String::New( g_value_get_string (&b) );
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
	(*obj)->Set(v8::String::NewSymbol((const char *)g_quark_to_string(field_id)), v );
	return true;
}

v8::Handle<v8::Object> gst_structure_to_v8( v8::Handle<v8::Object> obj, GstStructure *struc ) {
	const gchar *name = gst_structure_get_name(struc);
	obj->Set(v8::String::NewSymbol("name"), v8::String::New( name ) );
	gst_structure_foreach( struc, gst_structure_to_v8_value_iterate, &obj );
	return obj;
}



class GObjectWrap : public node::ObjectWrap {
	public:
		static void Init();
		static v8::Handle<v8::Value> NewInstance( const v8::Arguments& args, GObject *obj );

		void set( const char *name, const v8::Handle<v8::Value> value );
		
	private:
		GObjectWrap();
		~GObjectWrap();
		
		GObject *obj;
		
		static v8::Persistent<v8::Function> constructor;
		static v8::Handle<v8::Value> New(const v8::Arguments& args);
		static v8::Handle<v8::Value> _get(const v8::Arguments& args);
		static v8::Handle<v8::Value> _set(const v8::Arguments& args);

//		static v8::Handle<v8::Value> _onBufferAvailable(const v8::Arguments& args);
		static void _doPullBuffer( uv_work_t *req );
		static void _pulledBuffer( uv_work_t *req, int );
		static v8::Handle<v8::Value> _pull(const v8::Arguments& args);
};

v8::Persistent<v8::Function> GObjectWrap::constructor;

GObjectWrap::GObjectWrap() {
}

GObjectWrap::~GObjectWrap() {
}

void GObjectWrap::Init() {
	v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
	tpl->SetClassName(v8::String::NewSymbol("GObjectWrap"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("get"),
	  v8::FunctionTemplate::New(_get)->GetFunction());
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("set"),
	  v8::FunctionTemplate::New(_set)->GetFunction());

	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("pull"),
	  v8::FunctionTemplate::New(_pull)->GetFunction());

	constructor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
}

v8::Handle<v8::Value> GObjectWrap::New(const v8::Arguments& args) {
 	v8::HandleScope scope;

  	GObjectWrap* obj = new GObjectWrap();
	obj->Wrap(args.This());

	return args.This();
}

v8::Handle<v8::Value> GObjectWrap::NewInstance( const v8::Arguments& args, GObject *obj ) {
	v8::HandleScope scope;
	const unsigned argc = 1;
	v8::Handle<v8::Value> argv[argc] = { args[0] };
	v8::Local<v8::Object> instance = constructor->NewInstance(argc, argv);

	GObjectWrap* wrap = ObjectWrap::Unwrap<GObjectWrap>(instance);
  	wrap->obj = obj;

  	return scope.Close(instance);
}

v8::Handle<v8::Value> GObjectWrap::_get(const v8::Arguments& args) {
	v8::HandleScope scope;
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());

	v8::Local<v8::String> nameString = args[0]->ToString();
	v8::String::Utf8Value name( nameString );
	
	GObject *o = obj->obj;
    GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(o), *name );
    if( !spec ) {
		return v8::ThrowException( v8::Exception::ReferenceError(
				v8::String::Concat(
					v8::String::New("No such GObject property: "),
					nameString
				)));
    }

    GValue gv;
    memset( &gv, 0, sizeof( gv ) );
    g_value_init( &gv, G_PARAM_SPEC_VALUE_TYPE(spec) );
    g_object_get_property( o, *name, &gv );
        
    return scope.Close( gvalue_to_v8( &gv ) );
}

void GObjectWrap::set( const char *name, const v8::Handle<v8::Value> value ) {
	GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(obj), name );
	if( !spec ) {
		v8::ThrowException( v8::Exception::ReferenceError(
				v8::String::Concat(
					v8::String::New("No such GObject property: "),
					v8::String::New( name )
				)));
		return;
	}

	GValue gv;
	memset( &gv, 0, sizeof( gv ) );
	v8_to_gvalue( value, &gv );
	g_object_set_property( obj, name, &gv );
}

v8::Handle<v8::Value> GObjectWrap::_set(const v8::Arguments& args) {
	v8::HandleScope scope;
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
		return v8::ThrowException( v8::Exception::TypeError(v8::String::New("set expects name,value or object")) );
	}        
    return scope.Close( args[1] );
}

struct BufferRequest {
	uv_work_t request;
	v8::Persistent<v8::Function> cb_buffer, cb_caps;
	GstBuffer *buffer;
	GstCaps *caps;
	GObjectWrap *obj;
};

void GObjectWrap::_doPullBuffer( uv_work_t *req ) {
	BufferRequest *br = static_cast<BufferRequest*>(req->data);

	GstAppSink *sink = GST_APP_SINK(br->obj->obj);
	br->buffer = gst_app_sink_pull_buffer( sink );
}

void GObjectWrap::_pulledBuffer( uv_work_t *req, int n ) {
	BufferRequest *br = static_cast<BufferRequest*>(req->data);
	
	if( br->buffer ) {
		v8::HandleScope scope;

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

		v8::Persistent<v8::Value> buf = gstbuffer_to_v8( br->buffer );
		
		v8::Persistent<v8::Value> argv[1] = { buf };
		br->cb_buffer->Call(v8::Context::GetCurrent()->Global(), 1, argv);

		gst_buffer_unref(br->buffer);
		br->buffer = NULL;
		
		scope.Close( v8::Undefined() );
	}

	uv_queue_work( uv_default_loop(), &br->request, _doPullBuffer, _pulledBuffer );
}

v8::Handle<v8::Value> GObjectWrap::_pull( const v8::Arguments& args ) {
	v8::HandleScope scope;
	GObjectWrap* obj = ObjectWrap::Unwrap<GObjectWrap>(args.This());

	if( args.Length() < 2 || !args[0]->IsFunction() || !args[1]->IsFunction() ) {
		return scope.Close(v8::ThrowException(
			v8::Exception::Error(v8::String::New("Callbacks are required and must be Functions."))
		));
	}
  
    if( !GST_IS_APP_SINK( obj->obj ) ) {
		return scope.Close(v8::ThrowException( v8::Exception::TypeError(v8::String::New("not a GstAppSink")) ));
    }

	v8::Handle<v8::Function> cb_buffer = v8::Handle<v8::Function>::Cast(args[0]);
	v8::Handle<v8::Function> cb_caps = v8::Handle<v8::Function>::Cast(args[1]);
	
	BufferRequest * br = new BufferRequest();
	br->request.data = br;
	br->cb_buffer = v8::Persistent<v8::Function>::New( cb_buffer );
	br->cb_caps = v8::Persistent<v8::Function>::New( cb_caps );
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work( uv_default_loop(), &br->request, _doPullBuffer, _pulledBuffer );

	return scope.Close( v8::Undefined() );	
}


class Pipeline : public node::ObjectWrap {
	public:
		static void Init( v8::Handle<v8::Object> exports );
		
		void play();
		void pause();
		void stop();
		
		GObject *findChild( const char *name );
		v8::Handle<v8::Value> pollBus();
		
	private:
		Pipeline( const char *launch );
		~Pipeline();
		
		GstBin *pipeline;
		
		static v8::Handle<v8::Value> New(const v8::Arguments& args);
		static v8::Handle<v8::Value> _play(const v8::Arguments& args);
		static v8::Handle<v8::Value> _pause(const v8::Arguments& args);
		static v8::Handle<v8::Value> _stop(const v8::Arguments& args);
		static v8::Handle<v8::Value> _findChild(const v8::Arguments& args);

		static void _doPollBus( uv_work_t *req );
		static void _polledBus( uv_work_t *req, int );
		static v8::Handle<v8::Value> _pollBus(const v8::Arguments& args);
		
};

Pipeline::Pipeline( const char *launch ) {
	GError *err = NULL;

	pipeline = GST_BIN(gst_parse_launch(launch, &err));
	if( err ) {
		fprintf(stderr,"GstError: %s\n", err->message );
		v8::ThrowException( v8::String::New(err->message) );
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
		v8::Local<v8::Object> m = v8::Object::New();
		m->Set(v8::String::NewSymbol("type"), v8::String::New(GST_MESSAGE_TYPE_NAME(br->msg)) );
	
        if( br->msg->structure ) {
            gst_structure_to_v8( m, br->msg->structure );
        }

		if( GST_MESSAGE_TYPE(br->msg) == GST_MESSAGE_ERROR ) {
			GError *err = NULL;
			gchar *name;
			name = gst_object_get_path_string (br->msg->src);
			m->Set(v8::String::NewSymbol("name"), v8::String::New( name ) );
			gst_message_parse_error (br->msg, &err, NULL);
			m->Set(v8::String::NewSymbol("message"), v8::String::New( err->message) );
		}

		v8::Local<v8::Value> argv[1] = { m };
		br->callback->Call(v8::Context::GetCurrent()->Global(), 1, argv);
		
		gst_message_unref( br->msg );
	}
		
	uv_queue_work( uv_default_loop(), &br->request, _doPollBus, _polledBus );
}

v8::Handle<v8::Value> Pipeline::_pollBus( const v8::Arguments& args ) {
	v8::HandleScope scope;
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());

	if( args.Length() == 0 || !args[0]->IsFunction() ) {
		return scope.Close(v8::ThrowException(
			v8::Exception::Error(v8::String::New("Callback is required and must be a Function."))
		));
	}
  

	v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(args[0]);
	
	BusRequest * br = new BusRequest();
	br->request.data = br;
	br->callback = v8::Persistent<v8::Function>::New( callback );
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work( uv_default_loop(), &br->request, _doPollBus, _polledBus );

	return scope.Close( v8::Undefined() );	
}



void Pipeline::Init( v8::Handle<v8::Object> exports ) {
	v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
	tpl->SetClassName(v8::String::NewSymbol("Pipeline"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("play"),
	  v8::FunctionTemplate::New(_play)->GetFunction());
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("pause"),
	  v8::FunctionTemplate::New(_pause)->GetFunction());
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("stop"),
	  v8::FunctionTemplate::New(_stop)->GetFunction());
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("findChild"),
	  v8::FunctionTemplate::New(_findChild)->GetFunction());
	tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("pollBus"),
	  v8::FunctionTemplate::New(_pollBus)->GetFunction());

	v8::Persistent<v8::Function> constructor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
	exports->Set(v8::String::NewSymbol("Pipeline"), constructor);
}

v8::Handle<v8::Value> Pipeline::New(const v8::Arguments& args) {
	v8::HandleScope scope;

	v8::String::Utf8Value launch( args[0]->ToString() );

	Pipeline* obj = new Pipeline( *launch );
	obj->Wrap(args.This());

	return args.This();
}

v8::Handle<v8::Value> Pipeline::_play(const v8::Arguments& args) {
	v8::HandleScope scope;
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	obj->play();
	return scope.Close( v8::True() );
}
v8::Handle<v8::Value> Pipeline::_pause(const v8::Arguments& args) {
	v8::HandleScope scope;
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	obj->pause();
	return scope.Close( v8::True() );
}
v8::Handle<v8::Value> Pipeline::_stop(const v8::Arguments& args) {
	v8::HandleScope scope;
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());
	obj->stop();
	return scope.Close( v8::True() );
}
v8::Handle<v8::Value> Pipeline::_findChild(const v8::Arguments& args) {
	v8::HandleScope scope;
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());

	v8::String::Utf8Value name( args[0]->ToString() );
	GObject *o = obj->findChild( *name );
	if( o ) return scope.Close( GObjectWrap::NewInstance( args, o ) );

	return scope.Close( v8::Undefined() );
}
/*
v8::Handle<v8::Value> Pipeline::_pollBus(const v8::Arguments& args) {
	v8::HandleScope scope;
	Pipeline* obj = ObjectWrap::Unwrap<Pipeline>(args.This());

	return scope.Close( obj->pollBus() );
}
*/

void init( v8::Handle<v8::Object> exports ) {
	gst_init( NULL, NULL );
	GObjectWrap::Init();
	Pipeline::Init(exports);
}

NODE_MODULE(gstreamer_superficial, init)

