#ifndef __GLibHelpers_h__
#define __GLibHelpers_h__

#include <nan.h>
#include <gst/gst.h>

v8::Handle<v8::Object> createBuffer(char *data, int length);

v8::Handle<v8::Value> gstsample_to_v8( GstSample *sample );
v8::Handle<v8::Value> gstvaluearray_to_v8( const GValue *gv );
v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv );
void v8_to_gvalue( v8::Handle<v8::Value> v, GValue *gv );

gboolean gst_structure_to_v8_value_iterate( GQuark field_id, const GValue *val, gpointer user_data );
v8::Handle<v8::Object> gst_structure_to_v8( v8::Handle<v8::Object> obj, GstStructure *struc );


#endif
