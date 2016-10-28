#ifndef __Pipeline_h__
#define __Pipeline_h__

#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

class Pipeline : public Nan::ObjectWrap {
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

#endif
