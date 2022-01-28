#pragma warning(disable: 4819)
#include <gst/gst.h>

typedef struct {
	GstElement* pipeline, * src, * vconv, * vsink;
	GstElement* aconv, * arspl, * asink;
} DataStruct;

static void myhandler(GstElement* src, GstPad* src_pad, DataStruct* pd);

int main(int argc, char** argv) {

	DataStruct ds;
	GstBus* bus;
	GstMessage* msg;
	GstStateChangeReturn ret;
	gboolean terminate = FALSE;

	gst_init(&argc, &argv);

	ds.src = gst_element_factory_make("uridecodebin", "src");
	ds.vconv = gst_element_factory_make("videoconvert", "vconv");
	ds.vsink = gst_element_factory_make("autovideosink", "vsink");
	ds.aconv = gst_element_factory_make("audioconvert", "aconv");
	ds.arspl = gst_element_factory_make("audioresample", "arspl");
	ds.asink = gst_element_factory_make("autoaudiosink", "asink");
	ds.pipeline = gst_pipeline_new("pipeline");
	if (!ds.pipeline || !ds.src || !ds.vconv || !ds.vsink 
		|| !ds.aconv || !ds.arspl || !ds.asink) {
		g_printerr("elements creation failed.\n");
		return -1;
	}

	gst_bin_add_many(GST_BIN(ds.pipeline), ds.src, ds.vconv, ds.vsink, ds.aconv, ds.arspl, ds.asink, NULL);
	if (!gst_element_link(ds.vconv, ds.vsink)) {
		g_printerr("element vlinking failed.\n");
		gst_object_unref(ds.pipeline);
		return -1;
	}
	if (!gst_element_link_many(ds.aconv, ds.arspl, ds.asink, NULL)) {
		g_printerr("element alinking failed.\n");
		gst_object_unref(ds.pipeline);
		return -1;
	}

	g_object_set(ds.src, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);
	g_signal_connect(ds.src, "pad-added", G_CALLBACK(myhandler), &ds);
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

static void myhandler(GstElement* src, GstPad* src_pad, DataStruct* data) {
	GstPad* sink_pad = NULL;
	GstCaps* src_pad_caps = NULL;
	GstStructure* src_pad_structure = NULL;
	const gchar* src_pad_type = NULL;
	GstPadLinkReturn ret;

	g_print("-- %s from %s\n", GST_PAD_NAME(src_pad), GST_ELEMENT_NAME(src));

	if (gst_pad_is_linked(src_pad)) {
		g_print("-- this pad already linked.");
		goto exit;
	}

	src_pad_caps = gst_pad_get_current_caps(src_pad);
	src_pad_structure = gst_caps_get_structure(src_pad_caps, 0);
	src_pad_type = gst_structure_get_name(src_pad_structure);
	if (g_str_has_prefix(src_pad_type, "video/x-raw")) {
		sink_pad = gst_element_get_static_pad(data->vconv, "sink");
	} 
	else if (g_str_has_prefix(src_pad_type, "audio/x-raw")) {
		sink_pad = gst_element_get_static_pad(data->aconv, "sink");
	}
	else {
		g_printerr("unknown pad type: %s\n", src_pad_type);
		goto exit;
	}

	ret = gst_pad_link(src_pad, sink_pad);
	if (GST_PAD_LINK_FAILED(ret)) {
		g_print("-- pad linking failed. %s\n", src_pad_type);
		goto exit;
	}
	else {
		g_print("-- pad linking succeed. %s\n", src_pad_type);
	}

exit:
	if (src_pad_caps != NULL)
		gst_caps_unref(src_pad_caps);
	gst_object_unref(sink_pad);
}
