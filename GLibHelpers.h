#ifndef __GLibHelpers_h__
#define __GLibHelpers_h__

#include <nan.h>
#include <gst/gst.h>

using namespace v8;

Local<Object> createBuffer(char *data, int length);

Local<Value> gstsample_to_v8( GstSample *sample );
Local<Value> gstvaluearray_to_v8( const GValue *gv );
Local<Value> gvalue_to_v8( const GValue *gv );
void v8_to_gvalue( Local<Value> v, GValue *gv, GParamSpec *spec);

gboolean gst_structure_to_v8_value_iterate( GQuark field_id, const GValue *val, gpointer user_data );
Local<Object> gst_structure_to_v8( Local<Object> obj, const GstStructure *struc );


#endif
