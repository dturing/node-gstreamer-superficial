#ifndef __Pipeline_h__
#define __Pipeline_h__

#include <nan.h>
#include <gst/gst.h>

#include "GLibHelpers.h"

class Pipeline : public Nan::ObjectWrap {
	public:
		static void Init( Local<Object> exports );

		void play();
		void pause();
		void stop();
		gboolean seek(gint64 time_nanoseconds, GstSeekFlags flags);
		gint64 queryPosition();
		gint64 queryDuration();
		void sendEOS();
		void forceKeyUnit(GObject* sink, int cnt);

		GObject *findChild( const char *name );
		Local<Value> pollBus();

		void setPad( GObject* elem, const char *attribute, const char *padName );
		GObject *getPad( GObject* elem, const char *padName );

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
		static NAN_METHOD(Seek);
		static NAN_METHOD(QueryPosition);
		static NAN_METHOD(QueryDuration);
		static NAN_METHOD(SendEOS);
		static NAN_METHOD(ForceKeyUnit);
		static NAN_METHOD(FindChild);
		static NAN_METHOD(SetPad);
		static NAN_METHOD(GetPad);

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
