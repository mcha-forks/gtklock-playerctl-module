// gtklock-playerctl-module
// Copyright (c) 2022 Jovan Lanik

// Playerctl module

#include <playerctl.h>
#include <libsoup/soup.h>

#include "gtklock-module.h"

// TODO: simplify module data
#define MODULE_DATA(x) (x->module_data[self_id])
#define PLAYERCTL(x) ((struct playerctl *)MODULE_DATA(x))

extern void config_load(const char *path, const char *group, GOptionEntry entries[]);

struct playerctl {
	GtkWidget *revealer;
	GtkWidget *box;
};

const gchar module_name[] = "playerctl";
const gchar module_version[] = "v1.3.6";

static int self_id;

PlayerctlPlayerManager *player_manager = NULL;
PlayerctlPlayer *player = NULL;

static int art_size = 64;
static gchar *position = "top-center";

GOptionEntry module_entries[] = {
	{ "art-size", 0, 0, G_OPTION_ARG_INT, &art_size, NULL, NULL },
	{ "position", 0, 0, G_OPTION_ARG_STRING, &position, NULL, NULL },
	{ NULL },
};

static void setup_album_art(struct Window *ctx) {
	// TODO: Error messages
	// TODO: Async
	// TODO: URI from player
	if(!art_size) return;

	return;
	//gchar *uri = "file:///home/lanikjo/Downloads/th-407814961.jpg";
	gchar *uri = "https://upload.wikimedia.org/wikipedia/en/4/40/Bsmor.jpg";

	GError *error = NULL;

	SoupSession *session = soup_session_new();
	SoupRequest *request = soup_session_request(session, uri, &error);
	if(error != NULL) {
		g_warning("session: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	GInputStream *stream = soup_request_send(request, NULL, &error);
	if(error != NULL) {
		g_warning("request: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, -1, art_size, TRUE, NULL, &error);
	if(error != NULL) {
		g_warning("stream: %s", error->message);
		g_error_free(error);
	}

	GtkWidget *art = gtk_image_new_from_pixbuf(pixbuf);
	gtk_widget_set_halign(art, GTK_ALIGN_CENTER);
	gtk_widget_set_name(art, "album-art");
	gtk_box_pack_start(GTK_BOX(PLAYERCTL(ctx)->box), art, FALSE, FALSE, 0);
}

static void setup_playerctl(struct Window *ctx);

static void play_pause(GtkButton *self, gpointer user_data) {
	playerctl_player_play_pause(player, NULL);
	setup_playerctl((struct Window *)user_data);
}

// TODO: widget names
// TODO: comments
static void setup_playerctl(struct Window *ctx) {
	if(MODULE_DATA(ctx) != NULL) {
		gtk_widget_destroy(PLAYERCTL(ctx)->revealer);
		g_free(MODULE_DATA(ctx));
		MODULE_DATA(ctx) = NULL;
	}

	if(!player) return;
	PlayerctlPlaybackStatus status;
	g_object_get(player, "playback-status", &status, NULL);
	if(status == PLAYERCTL_PLAYBACK_STATUS_STOPPED) return;
	
	MODULE_DATA(ctx) = g_malloc(sizeof(struct playerctl));

	PLAYERCTL(ctx)->revealer = gtk_revealer_new();
	g_object_set(PLAYERCTL(ctx)->revealer, "margin", 5, NULL);
	gtk_widget_set_name(PLAYERCTL(ctx)->revealer, "playerctl-revealer");
	gtk_revealer_set_reveal_child(GTK_REVEALER(PLAYERCTL(ctx)->revealer), TRUE);
	gtk_revealer_set_transition_type(GTK_REVEALER(PLAYERCTL(ctx)->revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);

	if(
		g_strcmp0(position, "top-left") == 0 ||
		g_strcmp0(position, "bottom-left") == 0
	) gtk_widget_set_halign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_START);
	else if(
		g_strcmp0(position, "top-right") == 0 ||
		g_strcmp0(position, "bottom-right") == 0
	) gtk_widget_set_halign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_END);
	else gtk_widget_set_halign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_CENTER);

	if(
		g_strcmp0(position, "top-left") == 0 ||
		g_strcmp0(position, "top-right") == 0 ||
		g_strcmp0(position, "top-center") == 0
	) gtk_widget_set_valign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_START);
	else if(
		g_strcmp0(position, "bottom-left") == 0 ||
		g_strcmp0(position, "bottom-right") == 0 ||
		g_strcmp0(position, "bottom-center") == 0
	) gtk_widget_set_valign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_END);
	else if(g_strcmp0(position, "above-clock") != 0 && g_strcmp0(position, "under-clock") != 0) {
		g_warning_once("playerctl-module: Unknown position");
		gtk_widget_set_valign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_START);
		gtk_widget_set_halign(PLAYERCTL(ctx)->revealer, GTK_ALIGN_CENTER);
	} 

	gboolean under = g_strcmp0(position, "under-clock") == 0;
	if(under || g_strcmp0(position, "above-clock") == 0) {
		GValue val = G_VALUE_INIT;
		g_value_init(&val, G_TYPE_INT);
		gtk_container_child_get_property(GTK_CONTAINER(ctx->window_box), ctx->clock_label, "position", &val);
		gint pos = g_value_get_int(&val);

		gtk_container_add(GTK_CONTAINER(ctx->window_box), PLAYERCTL(ctx)->revealer);
		gtk_box_reorder_child(GTK_BOX(ctx->window_box), PLAYERCTL(ctx)->revealer, pos + under);
	}
	else gtk_overlay_add_overlay(GTK_OVERLAY(ctx->overlay), PLAYERCTL(ctx)->revealer);

	PLAYERCTL(ctx)->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
	gtk_container_add(GTK_CONTAINER(PLAYERCTL(ctx)->revealer), PLAYERCTL(ctx)->box);

	// TODO: Album art
	setup_album_art(ctx);

	GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_valign(label_box, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(PLAYERCTL(ctx)->box), label_box);

	gchar *title = playerctl_player_get_title(player, NULL);
	if(title && title[0] != '\0') {
		GtkWidget *title_label = gtk_label_new(NULL);
		gtk_widget_set_halign(title_label, GTK_ALIGN_START);
		// TODO: bold
		gtk_widget_set_name(title_label, "title-label");
		gtk_label_set_markup(GTK_LABEL(title_label), title);
		gtk_container_add(GTK_CONTAINER(label_box), title_label);
	}

	gchar *album = playerctl_player_get_album(player, NULL);
	if(album && album[0] != '\0') {
		GtkWidget *album_label = gtk_label_new(album);
		gtk_widget_set_halign(album_label, GTK_ALIGN_START);
		gtk_widget_set_name(album_label, "album-label");
		gtk_container_add(GTK_CONTAINER(label_box), album_label);
	}

	gchar *artist = playerctl_player_get_artist(player, NULL);
	if(artist && artist[0] != '\0') {
		GtkWidget *artist_label = gtk_label_new(artist);
		gtk_widget_set_halign(artist_label, GTK_ALIGN_START);
		gtk_widget_set_name(artist_label, "artist-label");
		gtk_container_add(GTK_CONTAINER(label_box), artist_label);
	}

	GtkWidget *control_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_valign(control_box, GTK_ALIGN_CENTER);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(control_box), GTK_BUTTONBOX_EXPAND);
	gtk_container_add(GTK_CONTAINER(PLAYERCTL(ctx)->box), control_box);

	GtkWidget *backward = gtk_button_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(control_box), backward);

	const gchar *icon = status == PLAYERCTL_PLAYBACK_STATUS_PLAYING ? "media-playback-pause" : "media-playback-start";
	GtkWidget *play_pause_button = gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON);
	g_signal_connect(play_pause_button, "clicked", G_CALLBACK(play_pause), ctx);
	gtk_container_add(GTK_CONTAINER(control_box), play_pause_button);

	GtkWidget *forward = gtk_button_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(control_box), forward);

	gtk_widget_show_all(PLAYERCTL(ctx)->revealer);
}

void g_module_unload(GModule *m) {
	g_object_unref(player_manager);
}

// TODO: signals
static void name_appeared(PlayerctlPlayerManager *self, PlayerctlPlayerName *name, gpointer user_data) {
	if(player) return;
	
	player = playerctl_player_new_from_name(name, NULL);
	playerctl_player_manager_manage_player(player_manager, player);
	g_object_unref(player);

	struct GtkLock *gtklock = user_data;
	if(gtklock->focused_window) setup_playerctl(gtklock->focused_window);
}

static void player_vanished(PlayerctlPlayerManager *self, PlayerctlPlayer *player, gpointer user_data) {
	player = NULL;
}

void on_activation(struct GtkLock *gtklock, int id) {
	self_id = id;
	config_load(gtklock->config_path, module_name, module_entries);

	GError *error = NULL;
	player_manager = playerctl_player_manager_new(&error);
	if(error != NULL) {
		g_warning("Playerctl failed: %s", error->message);
		g_error_free(error);
	} else {
		g_signal_connect(player_manager, "name-appeared", G_CALLBACK(name_appeared), gtklock);
		g_signal_connect(player_manager, "player-vanished", G_CALLBACK(player_vanished), NULL);

		GList *available_players = NULL;
		g_object_get(player_manager, "player-names", &available_players, NULL);
		if(available_players) {
			PlayerctlPlayerName *name = available_players->data;
			player = playerctl_player_new_from_name(name, NULL);
			playerctl_player_manager_manage_player(player_manager, player);
			g_object_unref(player);
		}
	}
}

void on_focus_change(struct GtkLock *gtklock, struct Window *win, struct Window *old) {
	setup_playerctl(win);
	if(gtklock->hidden)
		gtk_revealer_set_reveal_child(GTK_REVEALER(PLAYERCTL(win)->revealer), FALSE);
	if(old != NULL && win != old)
		gtk_revealer_set_reveal_child(GTK_REVEALER(PLAYERCTL(old)->revealer), FALSE);
}

void on_window_empty(struct GtkLock *gtklock, struct Window *ctx) {
	if(MODULE_DATA(ctx) != NULL) {
		g_free(MODULE_DATA(ctx));
		MODULE_DATA(ctx) = NULL;
	}
}

void on_idle_hide(struct GtkLock *gtklock) {
	if(gtklock->focused_window) {
		GtkRevealer *revealer = GTK_REVEALER(PLAYERCTL(gtklock->focused_window)->revealer);	
		gtk_revealer_set_reveal_child(revealer, FALSE);
	}
}

void on_idle_show(struct GtkLock *gtklock) {
	if(gtklock->focused_window) {
		GtkRevealer *revealer = GTK_REVEALER(PLAYERCTL(gtklock->focused_window)->revealer);	
		gtk_revealer_set_reveal_child(revealer, TRUE);
	}
}

