#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

v8::Handle<v8::Object> createBuffer(char *data, int length) {
	Nan::EscapableHandleScope scope;

	v8::Local<v8::Object> buffer = Nan::CopyBuffer(data, length).ToLocalChecked();

	return scope.Escape(buffer);
}

v8::Handle<v8::Value> gstsample_to_v8( GstSample *sample ) {
	GstMapInfo map;
	GstBuffer *buf = gst_sample_get_buffer( sample );
	if( gst_buffer_map( buf, &map, GST_MAP_READ ) ) {
		const unsigned char *data = map.data;
		int length = map.size;
		v8::Handle<v8::Object> frame = createBuffer( (char *)data, length );
		return frame;
	}

	return Nan::Undefined();
}

v8::Handle<v8::Value> gstvaluearray_to_v8( const GValue *gv ) {
	if( !GST_VALUE_HOLDS_ARRAY(gv) ) {
		Nan::ThrowTypeError("not a GstValueArray");
		return Nan::Undefined();
	}

	int size = gst_value_array_get_size(gv);
	v8::Handle<v8::Array> array = Nan::New<v8::Array>(gst_value_array_get_size(gv));

	for( int i=0; i<size; i++ ) {
		array->Set( Nan::New<v8::Number>(i), gvalue_to_v8(gst_value_array_get_value(gv,i)) );
	}
	return array;
}

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv ) {
	switch( G_VALUE_TYPE(gv) ) {
		case G_TYPE_STRING:
			return Nan::New<v8::String>( g_value_get_string(gv) ).ToLocalChecked();
		case G_TYPE_BOOLEAN:
			return Nan::New<v8::Boolean>( g_value_get_boolean(gv) );
		case G_TYPE_INT:
			return Nan::New<v8::Number>( g_value_get_int(gv) );
		case G_TYPE_UINT:
			return Nan::New<v8::Number>( g_value_get_uint(gv) );
		case G_TYPE_FLOAT:
			return Nan::New<v8::Number>( g_value_get_float(gv) );
		case G_TYPE_DOUBLE:
			return Nan::New<v8::Number>( g_value_get_double(gv) );
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
		return Nan::Undefined();
	}

//	printf("Value is of unhandled type %s\n", g_type_name( G_VALUE_TYPE(gv) ) );

	g_value_transform( gv, &b );

	const char *str = g_value_get_string( &b );
	if( str == NULL ) return Nan::Undefined();
	return Nan::New<v8::String>( g_value_get_string (&b) ).ToLocalChecked();
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
	v8::Local<v8::String> name = Nan::New<v8::String>((const char *)g_quark_to_string(field_id)).ToLocalChecked();
	(*obj)->Set(name, v);
	return true;
}

v8::Handle<v8::Object> gst_structure_to_v8( v8::Handle<v8::Object> obj, GstStructure *struc ) {
	const gchar *name = gst_structure_get_name(struc);
	obj->Set(Nan::New<v8::String>("name").ToLocalChecked(), Nan::New<v8::String>(name).ToLocalChecked());
	gst_structure_foreach( struc, gst_structure_to_v8_value_iterate, &obj );
	return obj;
}
