#ifndef __GObjectWrap_h__
#define __GObjectWrap_h__

#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

class GObjectWrap : public Nan::ObjectWrap {
	public:
		static void Init();
		static Local<Value> NewInstance( const Nan::FunctionCallbackInfo<Value>& info, GObject *obj );

		void set( const char *name, const Local<Value> value );

		void play();
		void pause();
		void stop();

	private:
		GObjectWrap() {}
		~GObjectWrap() {}

		GObject *obj;

		static Nan::Persistent<Function> constructor;
		static NAN_METHOD(New);
		static NAN_SETTER(SetProperty);
		static NAN_GETTER(GetProperty);

/*
		static void _doPullBuffer( uv_work_t *req );
		static void _pulledBuffer( uv_work_t *req, int );
		*/
		static NAN_METHOD(GstAppSinkPull);
        static NAN_METHOD(GstAppSrcPush);
};

#endif
