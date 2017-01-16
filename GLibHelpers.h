#ifndef __GLibHelpers_h__
#define __GLibHelpers_h__

#include <nan.h>
#include <gst/gst.h>

using namespace v8;

Handle<Object> createBuffer(char *data, int length);

Handle<Value> gstsample_to_v8( GstSample *sample );
Handle<Value> gstvaluearray_to_v8( const GValue *gv );
Handle<Value> gvalue_to_v8( const GValue *gv );
void v8_to_gvalue( Handle<Value> v, GValue *gv );

gboolean gst_structure_to_v8_value_iterate( GQuark field_id, const GValue *val, gpointer user_data );
Handle<Object> gst_structure_to_v8( Handle<Object> obj, const GstStructure *struc );


#endif
