#ifndef __Pipeline_h__
#define __Pipeline_h__

#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

class Pipeline : public Nan::ObjectWrap {
	public:
		static void Init( Handle<Object> exports );
		
		void play();
		void pause();
		void stop();
		void forceKeyUnit(GObject* sink, int cnt);
		
		GObject *findChild( const char *name );
		Handle<Value> pollBus();
		
	private:
		Pipeline(const char *launch);
		Pipeline(GstPipeline *pipeline);
		~Pipeline() {}
		
		static Nan::Persistent<Function> constructor;
		
		GstPipeline *pipeline;
		
		static NAN_METHOD(New);
		static NAN_METHOD(Play);
		static NAN_METHOD(Pause);
		static NAN_METHOD(Stop);
		static NAN_METHOD(ForceKeyUnit);
		static NAN_METHOD(FindChild);

		static void _doPollBus( uv_work_t *req );
		static void _polledBus( uv_work_t *req, int );
		static NAN_METHOD(PollBus);
		
		static NAN_GETTER(GetAutoFlushBus);
		static NAN_SETTER(SetAutoFlushBus);
		static NAN_GETTER(GetDelay);
		static NAN_SETTER(SetDelay);
		static NAN_GETTER(GetLatency);
		static NAN_SETTER(SetLatency);
		
};

#endif
