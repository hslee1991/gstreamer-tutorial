#pragma warning(disable: 4819)
#include <gst/gst.h>

typedef struct {
	GstElement* pipeline, * source, * convert, * sink;
} DataStruct;

static void myhandler(GstElement* source, GstPad* source_pad, DataStruct* pd);

int main(int argc, char** argv) {

	DataStruct ds;
	GstBus* bus;
	GstMessage* msg;
	GstStateChangeReturn ret;
	gboolean terminate = FALSE;

	gst_init(&argc, &argv);

	ds.source = gst_element_factory_make("uridecodebin", "source");
	ds.convert = gst_element_factory_make("videoconvert", "convert");
	ds.sink = gst_element_factory_make("autovideosink", "sink");
	ds.pipeline = gst_pipeline_new("pipeline");
	if (!ds.pipeline || !ds.source || !ds.convert || !ds.sink) {
		g_printerr("elements creation failed.\n");
		return -1;
	}

	gst_bin_add_many(GST_BIN(ds.pipeline), ds.source, ds.convert, ds.sink, NULL);
	if (!gst_element_link(ds.convert, ds.sink)) {
		g_printerr("element linking failed.\n");
		gst_object_unref(ds.pipeline);
		return -1;
	}

	g_object_set(ds.source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);
	g_signal_connect(ds.source, "pad-added", G_CALLBACK(myhandler), &ds);
	ret = gst_element_set_state(ds.pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("state changing failed.\n");
		gst_object_unref(ds.pipeline);
		return -1;
	}

	bus = gst_pipeline_get_bus(ds.pipeline);
	do {
		msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
			GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);
		if (msg != NULL) {
			GError* err;
			gchar* debug_info;

			switch (GST_MESSAGE_TYPE(msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &err, &debug_info);
				g_printerr("***** error received. %s: %s\n", GST_ELEMENT_NAME(msg->src), err->message);
				g_printerr("***** %s\n", debug_info ? debug_info : "none");
				g_clear_error(&err);
				g_free(debug_info);
				terminate = TRUE;
				break;
			case GST_MESSAGE_EOS:
				g_print("***** end of stream\n");
				terminate = TRUE;
				break;
			case GST_MESSAGE_STATE_CHANGED:
				if (GST_MESSAGE_SRC(msg) == GST_OBJECT(ds.pipeline)) {
					GstState oldstate, newstate, pending;
					gst_message_parse_state_changed(msg, &oldstate, &newstate, &pending);
					g_print("***** state changed from %s to %s\n",
						gst_element_state_get_name(oldstate), gst_element_state_get_name(newstate));
				}
				break;
			default:
				g_printerr("unexpected message received.\n");
				break;
			}
		}
		gst_message_unref(msg);
	} while (!terminate);

	gst_object_unref(bus);
	gst_element_set_state(ds.pipeline, GST_STATE_NULL);
	gst_object_unref(ds.pipeline);
	return 0;
}

static void myhandler(GstElement* source, GstPad* source_pad, DataStruct* pd) {
	GstPad* sink_pad = gst_element_get_static_pad(pd->convert, "sink");
	GstCaps* src_pad_caps = NULL;
	GstStructure* src_pad_structure = NULL;
	const gchar* src_pad_type = NULL;
	GstPadLinkReturn ret;

	g_print("-- %s from %s\n", GST_PAD_NAME(source_pad), GST_ELEMENT_NAME(source));

	if (gst_pad_is_linked(source_pad)) {
		g_print("-- this pad already linked.");
		goto exit;
	}

	src_pad_caps = gst_pad_get_current_caps(source_pad);
	src_pad_structure = gst_caps_get_structure(src_pad_caps, 0);
	src_pad_type = gst_structure_get_name(src_pad_structure);
	if (!g_str_has_prefix(src_pad_type, "video/x-raw")) {
		g_print("-- not my pad. %s\n", src_pad_type);
		goto exit;
	}

	ret = gst_pad_link(source_pad, sink_pad);
	if (GST_PAD_LINK_FAILED(ret)) {
		g_print("-- pad linking failed. %s\n", src_pad_type);
		goto exit;
	}

exit:
	if (src_pad_caps != NULL)
		gst_caps_unref(src_pad_caps);
	gst_object_unref(sink_pad);
}
