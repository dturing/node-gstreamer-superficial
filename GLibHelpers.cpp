#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

Handle<Object> createBuffer(char *data, int length) {
	Nan::EscapableHandleScope scope;

	Local<Object> buffer = Nan::CopyBuffer(data, length).ToLocalChecked();

	return scope.Escape(buffer);
}

Handle<Value> gstbuffer_to_v8(GstBuffer *buf) {
	if(!buf) return Nan::Null();
	GstMapInfo map;
	if(gst_buffer_map(buf, &map, GST_MAP_READ)) {
		const unsigned char *data = map.data;
		int length = map.size;
		Handle<Object> frame = createBuffer((char *)data, length);
		return frame;
	}
	return Nan::Undefined();
}

Handle<Value> gstsample_to_v8(GstSample *sample) {
	if(!sample) return Nan::Null();
	return gstbuffer_to_v8(gst_sample_get_buffer(sample));
}

Handle<Value> gstvaluearray_to_v8(const GValue *gv) {
	if(!GST_VALUE_HOLDS_ARRAY(gv)) {
		Nan::ThrowTypeError("not a GstValueArray");
		return Nan::Undefined();
	}

	int size = gst_value_array_get_size(gv);
	Handle<Array> array = Nan::New<Array>(gst_value_array_get_size(gv));

	for(int i=0; i<size; i++) {
		array->Set(Nan::New<Number>(i), gvalue_to_v8(gst_value_array_get_value(gv,i)));
	}
	return array;
}

Local<Value> gchararray_to_v8(const GValue *gv) {
	const char *str = g_value_get_string(gv);
	if(!str)
		return Nan::Null();
	return Nan::New(str).ToLocalChecked();
}

Handle<Value> gvalue_to_v8(const GValue *gv) {
	switch(G_VALUE_TYPE(gv)) {
		case G_TYPE_STRING:
			return gchararray_to_v8(gv);
		case G_TYPE_BOOLEAN:
			return Nan::New<Boolean>(g_value_get_boolean(gv));
		case G_TYPE_INT:
			return Nan::New<Number>(g_value_get_int(gv));
		case G_TYPE_UINT:
			return Nan::New<Number>(g_value_get_uint(gv));
		case G_TYPE_FLOAT:
			return Nan::New<Number>(g_value_get_float(gv));
		case G_TYPE_DOUBLE:
			return Nan::New<Number>(g_value_get_double(gv));
	}

	if(GST_VALUE_HOLDS_ARRAY(gv)) {
		return gstvaluearray_to_v8(gv);
	} else if(GST_VALUE_HOLDS_BUFFER(gv)) {
		GstBuffer *buf = gst_value_get_buffer(gv);
		return gstbuffer_to_v8(buf);
	}
	
	//printf("Value is of unhandled type %s\n", G_VALUE_TYPE_NAME(gv));

	/* Attempt to transform it into a GValue of type STRING */
	if(g_value_type_transformable (G_VALUE_TYPE(gv), G_TYPE_STRING)) {
		GValue b = G_VALUE_INIT;
		g_value_init(&b, G_TYPE_STRING);
		g_value_transform(gv, &b);
		return gchararray_to_v8(&b);
	}
	
	return Nan::Undefined();
}

void v8_to_gvalue(Handle<Value> v, GValue *gv) {
	if(v->IsNumber()) {
		g_value_init(gv, G_TYPE_FLOAT);
		g_value_set_float(gv, v->NumberValue());
	} else if(v->IsString()) {
		String::Utf8Value value(v->ToString());
		g_value_init(gv, G_TYPE_STRING);
		g_value_set_string(gv, *value);
	} else if(v->IsBoolean()) {
		g_value_init(gv, G_TYPE_BOOLEAN);
		g_value_set_boolean(gv, v->BooleanValue());
	}

	return;
}


/* --------------------------------------------------
    GstStructure helpers
   -------------------------------------------------- */
gboolean gst_structure_to_v8_value_iterate(GQuark field_id, const GValue *val, gpointer user_data) {
	Handle<Object> *obj = (Handle<Object>*)user_data;
	Handle<Value> v = gvalue_to_v8(val);
	Local<String> name = Nan::New<String>((const char *)g_quark_to_string(field_id)).ToLocalChecked();
	(*obj)->Set(name, v);
	return true;
}

Handle<Object> gst_structure_to_v8(Handle<Object> obj, const GstStructure *struc) {
	const gchar *name = gst_structure_get_name(struc);
	obj->Set(Nan::New("name").ToLocalChecked(), Nan::New(name).ToLocalChecked());
	gst_structure_foreach(struc, gst_structure_to_v8_value_iterate, &obj);
	return obj;
}
