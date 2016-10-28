#ifndef __GObjectWrap_h__
#define __GObjectWrap_h__

#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

class GObjectWrap : public Nan::ObjectWrap {
	public:
		static void Init();
		static v8::Handle<v8::Value> NewInstance( const Nan::FunctionCallbackInfo<v8::Value>& info, GObject *obj );

		void set( const char *name, const v8::Handle<v8::Value> value );

		void play();
		void pause();
		void stop();
		
	private:
		GObjectWrap();
		~GObjectWrap();
		
		GObject *obj;
		
		static Nan::Persistent<v8::Function> constructor;
		static NAN_METHOD(New);
		static NAN_METHOD(_get);
		static NAN_METHOD(_set);
		static NAN_METHOD(_play);
		static NAN_METHOD(_pause);
		static NAN_METHOD(_stop);

//		static v8::Handle<v8::Value> _onBufferAvailable(const v8::Arguments& info);
		static void _doPullBuffer( uv_work_t *req );
		static void _pulledBuffer( uv_work_t *req, int );
		static NAN_METHOD(_pull);
};

#endif
