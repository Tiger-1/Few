#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <gst/gst.h>





#define VIDEODIR "/home/hugo"

GstElement *pipeline, *source, *tee, *queue_r, *queue_d, *queue_f, *videoconvert, *parser, *encoder, *splitmuxsink, *timeoverlay, *clockoverlay, *videosink, *fakesink;
GstPad *src_pad, *sink_pad;
GstBus *bus;
GMainLoop *loop;
gulong id = 0, id2 = 0;
gchar flag = 0;
gchar f = 0;


static gboolean message_cb(GstBus *bus, GstMessage *message, gpointer user_data){
	
	switch (GST_MESSAGE_TYPE (message)) {
		
		case GST_MESSAGE_ERROR: {

			GError *err = NULL;
			gchar *name, *debug = NULL;
			name = gst_object_get_path_string (message->src);
			gst_message_parse_error (message, &err, &debug);
			g_printerr ("ERROR: from element %s: %s\n", name, err->message);
			if (debug != NULL)
				g_printerr("Additional debug info:\n%s\n", debug);
			g_error_free(err);
			g_free(debug);
			g_free(name);
			g_main_loop_quit(loop);
			break;
					}

		case GST_MESSAGE_WARNING: {

			GError *err = NULL;
			gchar *name, *debug = NULL;
			name = gst_object_get_path_string(message->src);
			gst_message_parse_warning (message, &err, &debug);
			g_printerr("WARNING: from element %s: %s\n", name, err->message);
			if(debug != NULL)
				g_printerr("Additional debug info:\n%s\n", debug);
			g_error_free(err);
			g_free(debug);
			g_free(name);
			break;
					  }

		case GST_MESSAGE_EOS: {

			g_print("EOS ARRIVED\n");
			g_main_loop_quit(loop);
			gst_element_set_state(pipeline, GST_STATE_NULL);
			g_main_loop_unref(loop);
			gst_object_unref(pipeline);
			sleep(1);
			exit(0);
			break;
				      }

		default:
			break;
	}

	return TRUE;
}


gchar * on_format_location(GstElement *object, guint id, gpointer user_data){

	g_print("formatting location. \n");
	time_t now = time(NULL);
	struct tm curr_tm = *localtime(&now);
	char video_dir[64];
	char date_dir[16];
	gchar *filename = (gchar *)malloc(100);

	strcpy(video_dir, VIDEODIR);
	if(access(video_dir, F_OK))
		mkdir(video_dir, 0775);

	sprintf(date_dir, "/%d%02d%02d", curr_tm.tm_year + 1900, curr_tm.tm_mon + 1, curr_tm.tm_mday);
	strcat(video_dir, date_dir);
	if(access(video_dir, F_OK))
		mkdir(video_dir, 0775);
	
	sprintf(filename, "%s/%d%02d%02d_%02d%02d%02d%s.mp4",video_dir, curr_tm.tm_year+1900, curr_tm.tm_mon+1, curr_tm.tm_mday, curr_tm.tm_hour, curr_tm.tm_min, curr_tm.tm_sec, curr_tm.tm_isdst? "DST": "");
	return filename;
}


gint camera_pause(){

	id = gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_IDLE, NULL, NULL, NULL);
       	id2 = gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_PULL, NULL, NULL, NULL);
	gst_pad_remove_probe(sink_pad, id2);	
	gst_pad_unlink(src_pad, sink_pad);
	gst_object_unref(sink_pad);


			
	gst_element_set_state(queue_d, GST_STATE_NULL);
	gst_element_set_state(videosink, GST_STATE_NULL);
			
	gst_object_ref( queue_d );
	gst_object_ref( videosink );
			
	gst_bin_remove_many(GST_BIN(pipeline), queue_d, videosink, NULL);
			
	gst_bin_add_many(GST_BIN(pipeline), queue_f, fakesink, NULL);
			
	gst_element_link( queue_f, fakesink);
			
	gst_element_sync_state_with_parent( queue_f );
	gst_element_sync_state_with_parent( fakesink );
			
	sink_pad = gst_element_get_static_pad( queue_f, "sink");
			
	gst_pad_link(src_pad, sink_pad);
			
	gst_pad_remove_probe(src_pad,id);
}

gint camera_play(){

	id = gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_IDLE, NULL, NULL, NULL);
   	gst_pad_unlink(src_pad, sink_pad);
	gst_object_unref(sink_pad);

        gst_element_set_state(queue_f, GST_STATE_NULL);
        gst_element_set_state(fakesink, GST_STATE_NULL);
                        	
//	gst_bin_add_many(GST_BIN(pipeline), queue_d, videosink, NULL);
	
	gst_object_ref( queue_f );
	gst_object_ref( fakesink );

	gst_bin_remove_many(GST_BIN(pipeline), queue_f, fakesink, NULL);
                        	
	gst_bin_add_many(GST_BIN(pipeline), queue_d, videosink, NULL);
                        	
	gst_element_link( queue_d, videosink);
                        	
	gst_element_sync_state_with_parent( queue_d );
        gst_element_sync_state_with_parent( videosink );
                        	
	sink_pad = gst_element_get_static_pad( queue_d, "sink");
                       	
	gst_pad_link(src_pad, sink_pad);
                        	
	gst_pad_remove_probe(src_pad,id);
}


void sigtstpHandler(int sig) {

      	if( 0 == flag ){
		camera_play();
		flag = 1;
	}
	else if( 1 == flag ){
		camera_pause();
		flag = 0;
	}
}



gint camera_init(){


	gst_init(NULL, NULL);
	pipeline = gst_pipeline_new(NULL);
	source = gst_element_factory_make("v4l2src", NULL);
	tee = gst_element_factory_make("tee", NULL);
	encoder = gst_element_factory_make("x264enc", NULL);
//	encoder = gst_element_factory_make("imxvpuenc_h264", NULL);
	parser = gst_element_factory_make("h264parse", NULL);
	videosink = gst_element_factory_make("autovideosink", NULL);
	fakesink = gst_element_factory_make("fakesink", NULL);
	splitmuxsink = gst_element_factory_make("splitmuxsink", NULL);
	videoconvert = gst_element_factory_make("videoconvert", NULL);
	timeoverlay = gst_element_factory_make("timeoverlay", NULL);
	clockoverlay = gst_element_factory_make("clockoverlay", NULL);
	queue_d	= gst_element_factory_make("queue", NULL);
	queue_r = gst_element_factory_make("queue", NULL);
	queue_f = gst_element_factory_make("queue", NULL);

	if( !pipeline || !source || !tee || !encoder || !parser || 
		!videosink || !fakesink || !splitmuxsink || !videoconvert || 
		!timeoverlay || !clockoverlay || !queue_r || !queue_d || !queue_f ){
	
		g_printerr("ERROR: element not found\n");
		gst_object_unref(pipeline);
		exit(1);
	}

//	g_object_set(G_OBJECT(source), "num-buffers", 500, NULL);
	g_object_set(G_OBJECT(splitmuxsink),"location","%d.mp4","max-size-time", 100000000000, NULL);
	g_object_set(G_OBJECT(encoder), "key-int-max", 10, "tune", 4, NULL); //encoder tuned to 0x04
	g_object_set(G_OBJECT(clockoverlay), "halignment", 0, "time-format", "%Y/%m/%d %H:%M:%S", NULL);
	g_object_set(G_OBJECT(timeoverlay), "halignment", 2, NULL);
	g_object_set(G_OBJECT(queue_r), "leaky", 2, NULL);
	g_object_set(G_OBJECT(queue_d), "leaky", 2, NULL);


	gst_bin_add_many(GST_BIN(pipeline), source, videoconvert, timeoverlay, clockoverlay,
			 tee, queue_r, queue_f, encoder, parser, fakesink, splitmuxsink, NULL);
	if(!gst_element_link_many(source, videoconvert, timeoverlay, clockoverlay, tee, NULL) || 
			!gst_element_link_many(tee, queue_r, encoder, parser, splitmuxsink, NULL) ||
			!gst_element_link_many(queue_f, fakesink, NULL)){
		g_printerr("ERROR: Unable to link elements\n");
		gst_object_unref(pipeline);
		exit(1);
	}

	g_signal_connect(splitmuxsink, "format-location", G_CALLBACK(on_format_location), NULL);
	
	
	sink_pad = gst_element_get_static_pad(queue_f, "sink");
        src_pad = gst_element_get_request_pad(tee,"src_%u");

        if(     gst_pad_link(src_pad,sink_pad) != GST_PAD_LINK_OK      ){

                g_printerr("Unable to link pads: Linking error\n");
                gst_object_unref(pipeline);
                exit(1);
        }

//        id = gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BLOCK, NULL, NULL, NULL);
//        flag = 1;
//        g_object_unref(src_pad);

//	sink_pad = gst_element_get_static_pad(queue_d, "sink");
//	src_pad = gst_element_get_request_pad(tee,"src_%u");

//	if( gst_pad_link(src_pad, sink_pad) != GST_PAD_LINK_OK ){
//		g_printerr("ERROR: Unable to link pads\n");
//		gst_object_unref(pipeline);
//		exit(1);
//	}
	
	return 0;
}

gint camera_deinit(){
	if( 1 == flag )
		camera_pause();
	gst_object_unref(queue_d);
	gst_object_unref(videosink);
	g_free(queue_d);
	g_free(videosink);
	gst_element_send_event(pipeline, gst_event_new_eos());
}

gint camera_record(){

	loop = g_main_loop_new( NULL, FALSE );
	bus = gst_pipeline_get_bus( GST_PIPELINE(pipeline) );
      	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(message_cb), NULL);
	gst_object_unref(GST_OBJECT(bus));
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run(loop);
	
	return 0;	
}

void sigintHandler(int sig){
	camera_deinit();
}

gint main(int argc, char *argv[]){
	signal(SIGTSTP,sigtstpHandler);
	signal(SIGINT,sigintHandler);
	camera_init(argc, argv);
	camera_record();
//	camera_deinit();
}
