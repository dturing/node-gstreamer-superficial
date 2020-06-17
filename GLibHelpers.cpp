#include <nan.h>
#include <gst/gst.h>
#include <gst/gstcaps.h>

#include "GLibHelpers.h"

Local<Object> createBuffer(char *data, int length) {
	Nan::EscapableHandleScope scope;

	Local<Object> buffer = Nan::CopyBuffer(data, length).ToLocalChecked();

	return scope.Escape(buffer);
}

Local<Value> gstbuffer_to_v8(GstBuffer *buf) {
	if(!buf) return Nan::Null();
	GstMapInfo map;
	if(gst_buffer_map(buf, &map, GST_MAP_READ)) {
		const unsigned char *data = map.data;
		int length = map.size;
		Local<Object> frame = createBuffer((char *)data, length);
		gst_buffer_unmap(buf, &map);
		return frame;
	}
	return Nan::Undefined();
}

Local<Value> gstsample_to_v8(GstSample *sample) {
	if(!sample) return Nan::Null();
	return gstbuffer_to_v8(gst_sample_get_buffer(sample));
}

Local<Value> gstvaluearray_to_v8(const GValue *gv) {
	if(!GST_VALUE_HOLDS_ARRAY(gv)) {
		Nan::ThrowTypeError("not a GstValueArray");
		return Nan::Undefined();
	}

	int size = gst_value_array_get_size(gv);
	Local<Array> array = Nan::New<Array>(gst_value_array_get_size(gv));

	for(int i=0; i<size; i++) {
		Nan::Set(array, i, gvalue_to_v8(gst_value_array_get_value(gv,i)));
	}
	return array;
}

Local<Value> gchararray_to_v8(const GValue *gv) {
	const char *str = g_value_get_string(gv);
	if(!str)
		return Nan::Null();
	return Nan::New(str).ToLocalChecked();
}

Local<Value> gvalue_to_v8(const GValue *gv) {
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
	} else if(GST_VALUE_HOLDS_SAMPLE(gv)) {
		GstSample *sample = gst_value_get_sample(gv);
		Local<Object> caps = Nan::New<Object>();
		GstCaps *gcaps = gst_sample_get_caps(sample);
		if (gcaps) {
			const GstStructure *structure = gst_caps_get_structure(gcaps,0);
			if (structure) gst_structure_to_v8(caps, structure);
		}

		Local<Object> result = Nan::New<Object>();
		Nan::Set(result, Nan::New("buf").ToLocalChecked(), gstsample_to_v8(sample));
		Nan::Set(result, Nan::New("caps").ToLocalChecked(), caps);
		return result;
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

void v8_to_gvalue(Local<Value> v, GValue *gv, GParamSpec *spec) {
	if(v->IsNumber()) {
		g_value_init(gv, G_TYPE_FLOAT);
		g_value_set_float(gv, v->ToNumber(Nan::GetCurrentContext()).ToLocalChecked()->Value());
	} else if(v->IsString()) {
        Nan::Utf8String value(v);
	    if(spec->value_type == GST_TYPE_CAPS) {
	        GstCaps* caps = gst_caps_from_string(*value);
	        g_value_init(gv, GST_TYPE_CAPS);
	        g_value_set_boxed(gv, caps);
	    } else {
	        g_value_init(gv, G_TYPE_STRING);
            g_value_set_string(gv, *value);
	    }
	} else if(v->IsBoolean()) {
		g_value_init(gv, G_TYPE_BOOLEAN);
		g_value_set_boolean(gv, Nan::To<v8::Boolean>(v).ToLocalChecked()->Value());
	}

	return;
}


/* --------------------------------------------------
    GstStructure helpers
   -------------------------------------------------- */
gboolean gst_structure_to_v8_value_iterate(GQuark field_id, const GValue *val, gpointer user_data) {
	Local<Object> *obj = (Local<Object>*)user_data;
	Local<Value> v = gvalue_to_v8(val);
	Local<String> name = Nan::New<String>((const char *)g_quark_to_string(field_id)).ToLocalChecked();
	Nan::Set(*obj, name, v);
	return true;
}

Local<Object> gst_structure_to_v8(Local<Object> obj, const GstStructure *struc) {
	const gchar *name = gst_structure_get_name(struc);
	Nan::Set(obj, Nan::New("name").ToLocalChecked(), Nan::New(name).ToLocalChecked());
	gst_structure_foreach(struc, gst_structure_to_v8_value_iterate, &obj);
	return obj;
}
