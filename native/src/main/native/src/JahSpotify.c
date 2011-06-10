#include <stdio.h>
#include <libspotify/api.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#include "JNIHelpers.h"
#include "JahSpotify.h"
#include "jahspotify_impl_JahSpotifyImpl.h"
#include "AppKey.h"
#include "audio.h"
#include "Callbacks.h"
#include "ThreadHelpers.h"

#define MAX_LENGTH_FOLDER_NAME 256

/// The global session handle
sp_session *g_sess;
/// Handle to the curren track
sp_track *g_currenttrack;

jobject g_playlistListener = NULL;
jobject g_connectionListener = NULL;
jobject g_playbackListener = NULL;


/// The output queue for audo data
static audio_fifo_t g_audiofifo;
/// Synchronization mutex for the main thread
static pthread_mutex_t g_notify_mutex;
/// Synchronization condition variable for the main thread
static pthread_cond_t g_notify_cond;
/// Synchronization variable telling the main thread to process events
static int g_notify_do;
/// Non-zero when a track has ended and the jukebox has not yet started a new one
static int g_playback_done;

jobject createJArtistInstance(JNIEnv *env, sp_artist *artist);
jobject createJAlbumInstance(JNIEnv *env, sp_album *album);
jobject createJTrackInstance(JNIEnv *env, sp_track *track);
jobject createJPlaylistInstance(JNIEnv *env, sp_playlist *playlist);

/* --------------------------  PLAYLIST CALLBACKS  ------------------------- */
/**
 * Callback from libspotify, saying that a track has been added to a playlist.
 *
 * @param  pl          The playlist handle
 * @param  tracks      An array of track handles
 * @param  num_tracks  The number of tracks in the \c tracks array
 * @param  position    Where the tracks were inserted
 * @param  userdata    The opaque pointer
 */
static void tracks_added ( sp_playlist *pl, sp_track * const *tracks,
                           int num_tracks, int position, void *userdata )
{
    /*
      int i;
        fprintf ( stderr,"jahspotify: %d tracks were added\n", num_tracks );

        if (sp_playlist_is_loaded(pl))
        {
            for (i = 0; i < num_tracks;i++)
            {
                sp_track *track = ( sp_track* ) tracks;
                if (sp_track_is_loaded( track ))
                {
                    fprintf ( stderr,"jahspotify: track: %s\n", sp_track_name ( track ));
                }
                tracks++;
            }
        }

        try_jukebox_start();
        */
}

/**
 * Callback from libspotify, saying that a track has been removed from a playlist.
 *
 * @param  pl          The playlist handle
 * @param  tracks      An array of track indices
 * @param  num_tracks  The number of tracks in the \c tracks array
 * @param  userdata    The opaque pointer
 */
static void tracks_removed ( sp_playlist *pl, const int *tracks,
                             int num_tracks, void *userdata )
{
    /*
        fprintf ( stderr,"jahspotify: %d tracks were removed\n", num_tracks );
        try_jukebox_start();
      */
}

/**
 * Callback from libspotify, telling when tracks have been moved around in a playlist.
 *
 * @param  pl            The playlist handle
 * @param  tracks        An array of track indices
 * @param  num_tracks    The number of tracks in the \c tracks array
 * @param  new_position  To where the tracks were moved
 * @param  userdata      The opaque pointer
 */
static void tracks_moved ( sp_playlist *pl, const int *tracks,
                           int num_tracks, int new_position, void *userdata )
{
    /*
        fprintf ( stderr,"jahspotify: %d tracks were moved around\n", num_tracks );
        try_jukebox_start();
        */
}

/**
 * Callback from libspotify. Something renamed the playlist.
 *
 * @param  pl            The playlist handle
 * @param  userdata      The opaque pointer
 */
static void playlist_renamed ( sp_playlist *pl, void *userdata )
{
    // fprintf ( stderr,"jahspotify: playlist renamed: %s\n",sp_playlist_name ( pl ));
}

static void playlist_state_changed ( sp_playlist *pl, void *userdata )
{
    /*sp_link *link = sp_link_create_from_playlist(pl);
    char *linkName = malloc(sizeof(char)*100);

    if (link)
    {
      sp_link_as_string(link,linkName, 100);
      fprintf ( stderr,"jahspotify: playlist state changed: %s link: %s (loaded: %s)\n",sp_playlist_name ( pl ), linkName, (sp_playlist_is_loaded(pl) ? "yes" : "no"));
      sp_link_release(link);
    }
    if (linkName) free(linkName);*/
}

static void playlist_update_in_progress ( sp_playlist *pl, bool done, void *userdata )
{
    const char *name = sp_playlist_name ( pl );
    char *playListlinkStr;
    char *trackLinkStr;
    sp_link *link;
    int trackCounter;

    // fprintf ( stderr,"jahspotify: playlist update in progress: %s (done: %s)\n",name, (done ? "yes" : "no"));
    if (done)
    {
        link = sp_link_create_from_playlist(pl);
        if (link)
        {
          playListlinkStr =  malloc ( sizeof ( char ) * ( 100 ) );
          sp_link_as_string(link,playListlinkStr,100);
          sp_link_release(link);
          signalPlaylistSeen(name,playListlinkStr);
	}
	
	

        //sp_link_release(link);
        //  int numTracks = sp_playlist_num_tracks(pl);
        //  fprintf ( stderr,"jahspotify: playlist: %s num tracks: %d id: %s\n",name,numTracks,playListlinkStr);

        //  for (trackCounter = 0 ; trackCounter < numTracks; trackCounter++)
        //  {
        //      sp_track *track = sp_playlist_track(pl,trackCounter);
        //      if (sp_track_is_loaded(track))
        //      {
        //	  link = sp_link_create_from_track(track,0);
        //	  trackLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
        //	  sp_link_as_string(link,trackLinkStr,100);
        //	  fprintf ( stderr,"jahspotify: track name: %s track id: %s\n",sp_track_name(track),trackLinkStr);
        //	  sp_link_release(link);
        //	  if (trackLinkStr) (trackLinkStr);
        //      }
        //      sp_track_release(track);
        //  }
        //
        //  if (playListlinkStr) free(playListlinkStr);
        // }
    }




}

static void playlist_metadata_updated ( sp_playlist *pl, void *userdata )
{
    // fprintf ( stderr,"jahspotify: playlist metadata updated: %s\n",sp_playlist_name ( pl ) );
}

/**
 * The callbacks we are interested in for individual playlists.
 */
static sp_playlist_callbacks pl_callbacks =
{
    .tracks_added = &tracks_added,
    .tracks_removed = &tracks_removed,
    .tracks_moved = &tracks_moved,
    .playlist_renamed = &playlist_renamed,
    .playlist_state_changed = &playlist_state_changed,
    .playlist_update_in_progress = &playlist_update_in_progress,
    .playlist_metadata_updated = &playlist_metadata_updated,

};


/* --------------------  PLAYLIST CONTAINER CALLBACKS  --------------------- */
/**
 * Callback from libspotify, telling us a playlist was added to the playlist container.
 *
 * We add our playlist callbacks to the newly added playlist.
 *
 * @param  pc            The playlist container handle
 * @param  pl            The playlist handle
 * @param  position      Index of the added playlist
 * @param  userdata      The opaque pointer
 */
static void playlist_added ( sp_playlistcontainer *pc, sp_playlist *pl,
                             int position, void *userdata )
{
    // fprintf(stderr,"jahspotify: playlist added: %s (loaded: %s)\n ",sp_playlist_name(pl),sp_playlist_is_loaded(pl) ? "Yes" : "No");
    // sp_playlist_add_callbacks ( pl, &pl_callbacks, NULL );
}

/**
 * Callback from libspotify, telling us a playlist was removed from the playlist container.
 *
 * This is the place to remove our playlist callbacks.
 *
 * @param  pc            The playlist container handle
 * @param  pl            The playlist handle
 * @param  position      Index of the removed playlist
 * @param  userdata      The opaque pointer
 */
static void playlist_removed ( sp_playlistcontainer *pc, sp_playlist *pl,
                               int position, void *userdata )
{
    const char *name = sp_playlist_name(pl);
    // fprintf(stderr,"jahspotify: playlist removed: %s\n ",name);
    sp_playlist_remove_callbacks ( pl, &pl_callbacks, NULL );
}


/**
 * Callback from libspotify, telling us the rootlist is fully synchronized
 *
 * @param  pc            The playlist container handle
 * @param  userdata      The opaque pointer
 */
static void container_loaded ( sp_playlistcontainer *pc, void *userdata )
{
    char *folderName = malloc ( sizeof ( char ) * ( MAX_LENGTH_FOLDER_NAME ) );
    int i;


    if ( folderName == NULL )
    {
        fprintf ( stderr, "jahspotify: Could not allocate folder name variable)\n" );
        return;
    }

    // fprintf ( stderr, "jahspotify: Rootlist synchronized (%d playlists)\n",sp_playlistcontainer_num_playlists ( pc ) );
    signalSynchStarting(sp_playlistcontainer_num_playlists (pc));

    for ( i = 0; i < sp_playlistcontainer_num_playlists ( pc ); ++i )
    {
        sp_playlist *pl = sp_playlistcontainer_playlist ( pc, i );
        sp_playlist_add_callbacks ( pl, &pl_callbacks, NULL );

        sp_link *link = sp_link_create_from_playlist(pl);

        char *linkStr = malloc(sizeof(char) * 100);
        if (link)
        {
            sp_link_add_ref(link);
            sp_link_as_string(link,linkStr,100);
        }
        else
        {
            strcpy(linkStr,"N/A\0");
        }
        switch ( sp_playlistcontainer_playlist_type ( pc,i ) )
        {
        case SP_PLAYLIST_TYPE_PLAYLIST:
            signalPlaylistSeen(sp_playlist_name ( pl ),linkStr);
            break;
        case SP_PLAYLIST_TYPE_START_FOLDER:
            sp_playlistcontainer_playlist_folder_name ( pc,i,folderName, MAX_LENGTH_FOLDER_NAME);
            signalStartFolderSeen(folderName, sp_playlistcontainer_playlist_folder_id(pc,i));
            break;
        case SP_PLAYLIST_TYPE_END_FOLDER:
            sp_playlistcontainer_playlist_folder_name ( pc,i,folderName,MAX_LENGTH_FOLDER_NAME);
            signalEndFolderSeen();
            break;
	case SP_PLAYLIST_TYPE_PLACEHOLDER:
	  fprintf ( stderr,"jahspotify: placeholder\n");
        }

        if (link)
            sp_link_release(link);
        free(linkStr);
    }
    signalSynchCompleted();
    free ( folderName );
}


/**
 * The playlist container callbacks
 */
static sp_playlistcontainer_callbacks pc_callbacks =
{
    .playlist_added = &playlist_added,
    .playlist_removed = &playlist_removed,
    .container_loaded = &container_loaded,
};



/* ---------------------------  SESSION CALLBACKS  ------------------------- */
/**
 * This callback is called when an attempt to login has succeeded or failed.
 *
 * @sa sp_session_callbacks#logged_in
 */
static void logged_in ( sp_session *sess, sp_error error )
{
    sp_playlistcontainer *pc = sp_session_playlistcontainer ( sess );
    int i;

    if ( SP_ERROR_OK != error )
    {
        fprintf ( stderr, "jahspotify: Login failed: %s\n", sp_error_message ( error ) );
        exit ( 2 );
    }

    fprintf ( stderr, "jahspotify: Login Success: %d\n", sp_playlistcontainer_num_playlists(pc));

    for (i = 0; i < sp_playlistcontainer_num_playlists(pc); ++i) {
        sp_playlist *pl = sp_playlistcontainer_playlist(pc, i);

        sp_playlist_add_callbacks(pl, &pl_callbacks, NULL);
    }

    signalLoggedIn();

}

static void logged_out ( sp_session *sess )
{
}


/**
 * This callback is called from an internal libspotify thread to ask us to
 * reiterate the main loop.
 *
 * We notify the main thread using a condition variable and a protected variable.
 *
 * @sa sp_session_callbacks#notify_main_thread
 */
static void notify_main_thread ( sp_session *sess )
{
    pthread_mutex_lock ( &g_notify_mutex );
    g_notify_do = 1;
    pthread_cond_signal ( &g_notify_cond );
    pthread_mutex_unlock ( &g_notify_mutex );
}

/**
 * This callback is used from libspotify whenever there is PCM data available.
 *
 * @sa sp_session_callbacks#music_delivery
 */
static int music_delivery ( sp_session *sess, const sp_audioformat *format,
                            const void *frames, int num_frames )
{   
  audio_fifo_t *af = &g_audiofifo;
	audio_fifo_data_t *afd;
	size_t s;
	
	if (num_frames == 0)
		return 0; // Audio discontinuity, do nothing


	pthread_mutex_lock(&af->mutex);

	/* Buffer one second of audio */
	if (af->qlen > format->sample_rate) 
	{
		pthread_mutex_unlock(&af->mutex);
		return 0;
	}

	s = num_frames * sizeof(int16_t) * format->channels;

	afd = malloc(sizeof(audio_fifo_data_t) + s);
	memcpy(afd->samples, frames, s);

	afd->nsamples = num_frames;

	afd->rate = format->sample_rate;
	afd->channels = format->channels;

	TAILQ_INSERT_TAIL(&af->q, afd, link);
	af->qlen += num_frames;

	pthread_cond_signal(&af->cond);

	pthread_mutex_unlock(&af->mutex);
	
	return num_frames;
}


/**
 * This callback is used from libspotify when the current track has ended
 *
 * @sa sp_session_callbacks#end_of_track
 */
static void end_of_track ( sp_session *sess )
{
    pthread_mutex_lock ( &g_notify_mutex );
    g_playback_done = 1;
    pthread_cond_signal ( &g_notify_cond );
    pthread_mutex_unlock ( &g_notify_mutex );
}


/**
 * Callback called when libspotify has new metadata available
 *
 * Not used in this example (but available to be able to reuse the session.c file
 * for other examples.)
 *
 * @sa sp_session_callbacks#metadata_updated
 */
static void metadata_updated ( sp_session *sess )
{
    fprintf ( stderr, "jahspotify: metadata updated\n" );
}

/**
 * Notification that some other connection has started playing on this account.
 * Playback has been stopped.
 *
 * @sa sp_session_callbacks#play_token_lost
 */
static void play_token_lost ( sp_session *sess )
{
    fprintf ( stderr, "jahspotify: play token lost\n" );
    if ( g_currenttrack != NULL )
    {
        sp_session_player_unload ( g_sess );
        g_currenttrack = NULL;
    }
}

static void userinfo_updated (sp_session *sess)
{
    printf("jukebox: user information updated\n");
}

static void log_message(sp_session *session, const char *data)
{
      fprintf(stderr,"jahspotify::log_message: %s\n",data);
}

static void connection_error(sp_session *session, sp_error error)
{
      fprintf(stderr,"jahspotify::connection_error: error=%s\n",sp_error_message(error));
      // FIXME: should propagate this to java land
}

static void streaming_error(sp_session *session, sp_error error)
{
      fprintf(stderr,"jahspotify::streaming_error: error=%s\n",sp_error_message(error));
      // FIXME: should propagate this to java land
}

static void start_playback(sp_session *session)
{
  fprintf(stderr,"jahspotify::start_playback: next playback about to start, initiating pre-load sequence\n");
  startPlaybackSignalled();
}

static void stop_playback(sp_session *session)
{
      fprintf(stderr,"jahspotify::stop_playback: playback should stop\n");
}


/**
 * The session callbacks
 */
static sp_session_callbacks session_callbacks =
{
    .logged_in = &logged_in,
    .logged_out = &logged_out,
    .notify_main_thread = &notify_main_thread,
    .music_delivery = &music_delivery,
    .metadata_updated = &metadata_updated,
    .play_token_lost = &play_token_lost,
    .log_message = log_message,
    .end_of_track = &end_of_track,
    .userinfo_updated = &userinfo_updated,
    .connection_error = &connection_error,
    .streaming_error = &streaming_error,
    .start_playback = &start_playback,
};


static sp_session_config spconfig =
{
    .api_version = SPOTIFY_API_VERSION,
    .cache_location = "/tmp",
    .settings_location = "/tmp",
    .application_key = g_appkey,
    .application_key_size = 0, // Set in main()
    .user_agent = "jahspotify/0.0.1",
    .callbacks = &session_callbacks,
    NULL,
};

JNIEXPORT jboolean JNICALL Java_jahspotify_impl_JahSpotifyImpl_registerPlaybackListener (JNIEnv *env, jobject obj, jobject playbackListener)
{
    g_playbackListener = (*env)->NewGlobalRef(env, playbackListener);
    fprintf ( stderr, "Registered playback listener: 0x%x\n", (int)g_playbackListener);
}

JNIEXPORT jboolean JNICALL Java_jahspotify_impl_JahSpotifyImpl_registerPlaylistListener (JNIEnv *env, jobject obj, jobject playlistListener)
{
    g_playlistListener = (*env)->NewGlobalRef(env, playlistListener);
    fprintf ( stderr, "Registered playlist listener: 0x%x\n", (int)g_playlistListener);
}

JNIEXPORT jboolean JNICALL Java_jahspotify_impl_JahSpotifyImpl_registerConnectionListener (JNIEnv *env, jobject obj, jobject connectionListener)
{
    g_connectionListener = (*env)->NewGlobalRef(env, connectionListener);
    fprintf ( stderr, "Registered connection listener: 0x%x\n", (int)g_connectionListener);
}

JNIEXPORT jboolean JNICALL Java_jahspotify_impl_JahSpotifyImpl_unregisterPlaylistListener (JNIEnv *env, jobject obj)
{
    if (g_playlistListener)
    {
        (*env)->DeleteGlobalRef(env, g_playlistListener);
        g_playlistListener = NULL;
    }
}


JNIEXPORT jboolean JNICALL Java_jahspotify_impl_JahSpotifyImpl_unregisterConnectionListener (JNIEnv *env, jobject obj)
{
    if (g_connectionListener)
    {
        (*env)->DeleteGlobalRef(env, g_connectionListener);
        g_connectionListener = NULL;
    }
}

JNIEXPORT jobject JNICALL Java_jahspotify_impl_JahSpotifyImpl_retrieveUser (JNIEnv *env, jobject obj)
{
    sp_user *user = sp_session_user(g_sess);
    const char *value = NULL;
    int country = 0;

    while (!sp_user_is_loaded(user))
    {
        fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveUser: waiting for user to load\n" );
	sleep(1);
    }
    
    jclass jClass = (*env)->FindClass(env, "jahspotify/media/User");
    if (jClass == NULL)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveUser: could not load jahnotify.media.User\n");
      return NULL;
    }
 
    jobject userInstance = (*env)->AllocObject(env,jClass);
    if (!userInstance)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveUser: could not create instance of jahspotify.media.User\n");
      return NULL;
    }
    
    while (!sp_user_is_loaded(user))
    {
      sleep(1);
    }
    
    if (sp_user_is_loaded(user))
    {
        fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveUser: user is loaded\n" );
        value = sp_user_full_name(user);
        if (value)
        {
            setObjectStringField(env,userInstance,"_fullName",value);
        }
        value = sp_user_canonical_name(user);
        if (value)
        {
            setObjectStringField(env,userInstance,"_userName",value);
        }
        value = sp_user_display_name(user);
        if (value)
        {
            setObjectStringField(env,userInstance,"_displayName",value);
        }
        value = sp_user_picture(user);
        if (value)
        {
            setObjectStringField(env,userInstance,"_imageURL",value);
        }

        // Country encoded in an integer 'SE' = 'S' << 8 | 'E'
        country = sp_session_user_country(g_sess);
        char countryStr[] = "  ";
        countryStr[0] = (byte)(country >> 8);
        countryStr[1] = (byte)country;
        setObjectStringField(env,userInstance,"_country",countryStr);

        return userInstance;
    }

    return NULL;

}

JNIEXPORT jobjectArray JNICALL Java_jahspotify_impl_JahSpotifyImpl_getTracksForPlaylist ( JNIEnv *env, jobject obj, jstring uri)
{
  jstring str = NULL;
  int i;
  jclass strCls = (*env)->FindClass(env,"Ljava/lang/String;");
  jobjectArray strarray = (*env)->NewObjectArray(env,6,strCls,NULL);
  for(i=0;i<6;i++)
  {
    str = (*env)->NewStringUTF(env,"VIPIN");
    (*env)->SetObjectArrayElement(env,strarray,i,str);
    (*env)->DeleteLocalRef(env,str);
  }
  return strarray;
  
}

jobject createJTrackInstance(JNIEnv *env, sp_track *track)
{
  jclass jClass;
  jobject trackInstance;
  
  jClass = (*env)->FindClass(env, "jahspotify/media/Track");
  if (jClass == NULL)
  {
    fprintf(stderr,"jahspotify::createJTrackInstance: could not load jahnotify.media.Track\n");
    return NULL;
  }
 
  trackInstance = (*env)->AllocObject(env,jClass);
  if (!trackInstance)
  {
    fprintf(stderr,"jahspotify::createJTrackInstance: could not create instance of jahspotify.media.Track\n");
    return NULL;
  }

  sp_link *trackLink = sp_link_create_from_track(track,0);
  
  if (trackLink)
  {
    sp_link_add_ref(trackLink);
    char *trackLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
    sp_link_as_string(trackLink,trackLinkStr,100);

    setObjectStringField(env,trackInstance,"id",trackLinkStr);

    sp_link_release(trackLink);
    free(trackLinkStr);
    
    setObjectStringField(env,trackInstance,"title",sp_track_name(track));
    setObjectIntField(env,trackInstance,"length",sp_track_duration(track));
    setObjectIntField(env,trackInstance,"popularity",sp_track_popularity(track));
    setObjectIntField(env,trackInstance,"trackNumber",sp_track_index(track));
    
    sp_album *album = sp_track_album(track);
    if (album)
    {
      while (!sp_album_is_loaded(album))
      {
	// Wait for it!
	fprintf(stderr,"jahspotify::createJTrackInstance: waiting for album instance ... \n");
	sleep(1);
      }
      
      sp_album_add_ref(album);
      if (sp_album_is_loaded(album))
      {
        setObjectIntField(env,trackInstance,"year",sp_album_year(album));
	
	jobject albumInstance = createJAlbumInstance(env,album);

	// Lookup the method now - saves us looking it up for each iteration of the loop
	jmethodID jMethod = (*env)->GetMethodID(env,jClass,"setAlbum","(Ljahspotify/media/Album;)V");
	
	if (jMethod == NULL)
	{
	  fprintf(stderr,"jahspotify::createJTrackInstance: could not load method setAlbum(album) on class Track\n");
	  return NULL;
	}
	
        // set it on the track
        (*env)->CallVoidMethod(env,trackInstance,jMethod,albumInstance);
	
	sp_artist *artist = sp_album_artist(album);
	if (artist)
	{
	   while (!sp_artist_is_loaded(artist))
           {
	     // Wait for it!
	     fprintf(stderr,"jahspotify::createJTrackInstance: waiting for artist instance ... \n");
	     sleep(1);
	   }
      
	  sp_artist_add_ref(artist);
	  if (sp_artist_is_loaded(artist))
	  {
	    jobject artistInstance = createJArtistInstance(env,artist);
	    // Lookup the method now - saves us looking it up for each iteration of the loop
	    jmethodID jMethod = (*env)->GetMethodID(env,jClass,"setArtist","(Ljahspotify/media/Artist;)V");
	    
	    if (jMethod == NULL)
	    {
	      fprintf(stderr,"jahspotify::createJTrackInstance: could not load method setArtist(artist) on class Track\n");
	      return NULL;
	      
	    }
	    // set it on the track
	    (*env)->CallVoidMethod(env,trackInstance,jMethod,artistInstance);
	  }
	  sp_artist_release(artist);
	}
      }
      sp_album_release(album);
    }
  }

  return trackInstance;
  
}

jobject createJAlbumInstance(JNIEnv *env, sp_album *album)
{
  jclass jClass;
  jobject albumInstance;
  
  jClass = (*env)->FindClass(env, "jahspotify/media/Album");
  if (jClass == NULL)
  {
    fprintf(stderr,"jahspotify::createJAlbumInstance: could not load jahnotify.media.Album\n");
    return NULL;
  }
 
  albumInstance = (*env)->AllocObject(env,jClass);
  if (!albumInstance)
  {
    fprintf(stderr,"jahspotify::createJAlbumInstance: could not create instance of jahspotify.media.Album\n");
    return NULL;
  }

  sp_link *albumLink = sp_link_create_from_album(album);
  
  if (albumLink)
  {
    sp_link_add_ref(albumLink);
    char *albumLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
    sp_link_as_string(albumLink,albumLinkStr,100);

    setObjectStringField(env,albumInstance,"id",albumLinkStr);

    sp_link_release(albumLink);
    free(albumLinkStr);
    
    setObjectStringField(env,albumInstance,"name",sp_album_name(album));
    setObjectIntField(env,albumInstance,"year",sp_album_year(album));

    const byte *coverBytes = sp_album_cover(album);

    setObjectStringField(env,albumInstance,"id",albumLinkStr);

    sp_link *albumCoverLink = sp_link_create_from_album_cover(album);
    if (albumCoverLink)
    {
      char *albumCoverLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
      sp_link_as_string(albumCoverLink,albumCoverLinkStr,100);
      setObjectStringField(env,albumInstance,"cover",albumCoverLinkStr);
      sp_link_release(albumCoverLink);
      free(albumCoverLinkStr);
    }
        
  }
  return albumInstance;
  
}

jobject createJArtistInstance(JNIEnv *env, sp_artist *artist)
{
  jclass jClass;
  jobject artistInstance = createInstance(env,"jahspotify/media/Artist");
  sp_link *artistLink = sp_link_create_from_artist(artist);
  
  if (artistLink)
  {
    sp_link_add_ref(artistLink);
    char *artistLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
    sp_link_as_string(artistLink,artistLinkStr,100);

    setObjectStringField(env,artistInstance,"id",artistLinkStr);

    sp_link_release(artistLink);
    free(artistLinkStr);
    
    setObjectStringField(env,artistInstance,"name",sp_artist_name(artist));
  }
  return artistInstance;
  
}


jobject createJPlaylist(JNIEnv *env, sp_playlist *playlist)
{
  jclass jClass;
  jobject playlistInstance;
  jmethodID jMethod;
  
  jClass = (*env)->FindClass(env, "jahspotify/media/Playlist");
  if (jClass == NULL)
  {
    fprintf(stderr,"jahspotify::createJPlaylist: could not load jahnotify.media.Playlist\n");
    return NULL;
  }
 
  playlistInstance = (*env)->AllocObject(env,jClass);
  if (!playlistInstance)
  {
    fprintf(stderr,"jahspotify::createJPlaylist: could not create instance of jahspotify.media.Playlistt\n");
    return NULL;
  }
  
  sp_link *playlistLink = sp_link_create_from_playlist(playlist);
  if (playlistLink)
  {
    char *playlistLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
    sp_link_as_string(playlistLink,playlistLinkStr,100);
    setObjectStringField(env,playlistInstance,"id",playlistLinkStr);
    free (playlistLinkStr);
    sp_link_release(playlistLink);
  }
  
  setObjectStringField(env,playlistInstance,"name",sp_playlist_name(playlist));
  
  sp_user *owner = sp_playlist_owner(playlist);
  if (owner)
  {
    setObjectStringField(env,playlistInstance,"author",sp_user_display_name(owner));
    sp_user_release(owner);
  } 

  // Lookup the method now - saves us looking it up for each iteration of the loop
  jMethod = (*env)->GetMethodID(env,jClass,"addTrack","(Ljahspotify/media/Track;)V");
      
  if (jMethod == NULL)
  {
    fprintf(stderr,"jahspotify::createJPlaylist: could not load method addTrack(track) on class Playlist\n");
    return NULL;
  }

  int numTracks = sp_playlist_num_tracks(playlist);
  int trackCounter = 0;
  for (trackCounter = 0 ; trackCounter < numTracks; trackCounter++)
  {
    sp_track *track = sp_playlist_track(playlist,trackCounter);
    if (track)
    {
      sp_track_add_ref(track);

      while (!sp_track_is_loaded(track))
      {
	// Wait for it!
        fprintf(stderr,"jahspotify::createJPlaylist: waiting for track instance to load\n");
	sleep(1);
      }

      jobject trackInstance = createJTrackInstance(env,track);

      // Add it to the playlist
      (*env)->CallVoidMethod(env,playlistInstance,jMethod,trackInstance);

      sp_track_release(track);
      
    }
  }

  return playlistInstance;
  
}



JNIEXPORT jobject JNICALL Java_jahspotify_impl_JahSpotifyImpl_retrieveAlbum ( JNIEnv *env, jobject obj, jstring uri)
{
  jobject albumInstance;
  uint8_t *nativeUri = NULL;
  
  nativeUri = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, uri, NULL );
  
  sp_link *link = sp_link_create_from_string(nativeUri);
  if (!link)
  {
    // hmm
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveAlbum: Could not create link!\n" );
    return JNI_FALSE;
  }

  sp_album *album= sp_link_as_album(link);
  
  while (!sp_album_is_loaded(album))
  {
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveAlbum: Waiting for album to be loaded ...\n" );
    sleep(1);
  }

  albumInstance = createJAlbumInstance(env, album);
  
  if (album)
    sp_album_release(album);
  if (link)
    sp_link_release(link);
  if (nativeUri)
    free(nativeUri);
  
  fprintf ( stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveAlbum: returning track: 0x%x\n",(unsigned int)albumInstance);

  return albumInstance;
}

JNIEXPORT jobject JNICALL Java_jahspotify_impl_JahSpotifyImpl_retrieveTrack ( JNIEnv *env, jobject obj, jstring uri)
{
  jobject trackInstance;
  uint8_t *nativeUri = NULL;
  
  nativeUri = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, uri, NULL );
  
  sp_link *link = sp_link_create_from_string(nativeUri);
  if (!link)
  {
    // hmm
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveTrack: Could not create link!\n" );
    return JNI_FALSE;
  }

  sp_track *track = sp_link_as_track(link);
  
  while (!sp_track_is_loaded(track))
  {
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveTrack: Waiting for track to be loaded ...\n" );
    sleep(1);
  }

  trackInstance = createJTrackInstance(env, track);
  
  if (track)
    sp_track_release(track);
  if (link)
    sp_link_release(link);
  if (nativeUri)
    free(nativeUri);
  
  fprintf ( stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrieveTrack: returning track: 0x%x\n",(unsigned int)trackInstance);

  return trackInstance;
}

JNIEXPORT jobject JNICALL Java_jahspotify_impl_JahSpotifyImpl_retrievePlaylist ( JNIEnv *env, jobject obj, jstring uri)
{
  jobject playlistInstance;
  uint8_t *nativeUri = NULL;
  
  nativeUri = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, uri, NULL );
  
  sp_link *link = sp_link_create_from_string(nativeUri);
  if (!link)
  {
    // hmm
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrievePlaylist: Could not create link!\n" );
    return JNI_FALSE;
  }

  sp_playlist *playlist = sp_playlist_create(g_sess,link);
  
  while (!sp_playlist_is_loaded(playlist))
  {
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_retrievePlaylist: Waiting for playlist to be loaded ...\n" );
    sleep(1);
  }

  playlistInstance = createJPlaylist(env, playlist);
  
  if (playlist)
    sp_playlist_release(playlist);
  if (link)
    sp_link_release(link);
  if (nativeUri)
    free(nativeUri);
  
  return playlistInstance;
  
}


JNIEXPORT jboolean JNICALL Java_jahspotify_impl_JahSpotifyImpl_populatePlaylist ( JNIEnv *env, jobject obj, jstring uri, jobject playlistInstance)
{
  uint8_t *nativeUri = NULL;
  nativeUri = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, uri, NULL );

  sp_link *link = sp_link_create_from_string(nativeUri);
  if (!link)
  {
    // hmm
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_populatePlaylist: Could not create link!\n" );
    return JNI_FALSE;
  }

  sp_playlist *playlist = sp_playlist_create(g_sess,link);
  
  while (!sp_playlist_is_loaded(playlist))
  {
    fprintf ( stderr, "jahspotify::Java_jahspotify_impl_JahSpotifyImpl_populatePlaylist: Waiting for playlist to be loaded ...\n" );
    sleep(1);
  }
  
  setObjectStringField(env,playlistInstance,"name",sp_playlist_name(playlist));
  setObjectStringField(env,playlistInstance,"id",nativeUri);
  
  sp_user *owner = sp_playlist_owner(playlist);
  if (owner)
  {
    setObjectStringField(env,playlistInstance,"author",sp_user_display_name(owner));
    sp_user_release(owner);
  } 
  int numTracks = sp_playlist_num_tracks(playlist);
  int trackCounter = 0;
  for (trackCounter = 0 ; trackCounter < numTracks; trackCounter++)
  {
    sp_track *track = sp_playlist_track(playlist,trackCounter);
    if (sp_track_is_loaded(track))
    {
      sp_link *trackLink = sp_link_create_from_track(track,0);
      if (trackLink)
      {
        char *trackLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
        sp_link_as_string(trackLink,trackLinkStr,100);
        fprintf ( stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_populatePlaylist track name: %s track id: %s\n",sp_track_name(track),trackLinkStr);
	sp_album *album = sp_track_album(track);
	if (sp_album_is_loaded(album))
	{
	  sp_artist *artist = sp_album_artist(album);
	  if (sp_artist_is_loaded(artist))
	  {
	    fprintf ( stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_populatePlaylist artist: %s\n",sp_artist_name(artist));
	  }
	}
        sp_link_release(trackLink);
	free(trackLinkStr);
      }
    }
    sp_track_release(track);
  }
  
  if (playlist)
    sp_playlist_release(playlist);
  if (link)
    sp_link_release(link);
  if (nativeUri)
    free(nativeUri);
  
  fprintf ( stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_populatePlaylist: returning playlist: 0x%x\n",(unsigned int)playlist);

  return JNI_TRUE;
  
}

JNIEXPORT jobjectArray JNICALL Java_jahspotify_impl_JahSpotifyImpl_nativeReadTracks (JNIEnv *env, jobject obj, jobjectArray uris)
{
  // For each track, read out the info and populate all of the info in the Track instance
}

JNIEXPORT int JNICALL Java_jahspotify_impl_JahSpotifyImpl_pause (JNIEnv *env, jobject obj)
{
  if (g_currenttrack)
  {
    sp_session_player_play(g_sess,0);
  }
}

JNIEXPORT int JNICALL Java_jahspotify_impl_JahSpotifyImpl_resume (JNIEnv *env, jobject obj)
{
  if (g_currenttrack)
  {
    sp_session_player_play(g_sess,1);
  }
}

JNIEXPORT int JNICALL Java_jahspotify_impl_JahSpotifyImpl_readImage (JNIEnv *env, jobject obj, jstring uri, jobject outputStream)
{
  uint8_t *nativeURI = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, uri, NULL );
  sp_link *imageLink = sp_link_create_from_string(nativeURI);
  size_t size;
  jclass jClass;
  

  if (imageLink)
  {
    sp_image *image = sp_image_create_from_link(g_sess,imageLink);
    while (!sp_image_is_loaded(image))
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_readImage: Waiting for image to load ...\n");
      sleep(1);
    }
    byte *data = (byte*)sp_image_data(image,&size);
    fprintf(stderr,"Image size: %d\n", size);

    jClass = (*env)->FindClass(env,"Ljava/io/OutputStream;");
    if (jClass == NULL)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_readImage: could not load class java.io.OutputStream\n");
      return NULL;
    }
    // Lookup the method now - saves us looking it up for each iteration of the loop
    jmethodID jMethod = (*env)->GetMethodID(env,jClass,"write","(I)V");
    
    if (jMethod == NULL)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_readImage: could not load method write(int) on class java.io.OutputStream\n");
      return NULL;
    }
    
    int i = 0;
    for (i = 0; i < size; i++)
    {
      (*env)->CallVoidMethod(env,outputStream,jMethod,*data);
      data++;
    }
    
    sp_link_release(imageLink);
  }
  
}

JNIEXPORT int JNICALL Java_jahspotify_impl_JahSpotifyImpl_play (JNIEnv *env, jobject obj, jstring uri)
{
  uint8_t *nativeURI = NULL;
  
  fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Initiating play\n");

  nativeURI = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, uri, NULL );

  fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: uri: %s\n", nativeURI);
  
  // For each track, read out the info and populate all of the info in the Track instance
  sp_link *link = sp_link_create_from_string(nativeURI);
  if (link)
  {
    sp_track *t = sp_link_as_track(link);

    if (!t)
    {
      fprintf(stderr,"No track from link\n");
      return;
    }

    while (!sp_track_is_available(g_sess,t))
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Waiting for track ...\n");
      sleep(1);
    }      

    if (sp_track_error(t) != SP_ERROR_OK)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Error with track: %s\n",sp_error_message(sp_track_error(t)));

      return;
    }

    fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: name: %s duration: %d\n",sp_track_name(t),sp_track_duration(t));

    if (g_currenttrack == t)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Same track!\n");
      return;
    }
    
    // If there is one playing, unload that now
    if (g_currenttrack && t != g_currenttrack)
    {
      // audio_fifo_flush(&g_audiofifo);

      // Unload the current track now
      sp_session_player_unload(g_sess);

      sp_link *currentTrackLink = sp_link_create_from_track(g_currenttrack,0);
      char *currentTrackLinkStr = NULL;
      if (currentTrackLink)
      {
        currentTrackLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
	sp_link_as_string(currentTrackLink,currentTrackLinkStr,100);
	sp_link_release(currentTrackLink);
      }
      
      signalTrackEnded(currentTrackLinkStr, JNI_TRUE);
      
      if (currentTrackLinkStr)
      {
	free(currentTrackLinkStr);
      }

      sp_track_release(g_currenttrack);
      
      g_currenttrack = NULL;

    }
    else
    {
      // audio_fifo_flush(&g_audiofifo);
      // audio_close();
      // audio_init(&g_audiofifo);
    }
    
    sp_track_add_ref(t);

    sp_error result = sp_session_player_load(g_sess, t);
    
    if (sp_track_error(t) != SP_ERROR_OK)
    {
      fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Issue loading track: %s\n", sp_error_message((sp_track_error(t))));
    }
    
    fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Track loaded: %s\n", (result == SP_ERROR_OK ? "yes" : "no"));

    // Update the global reference
    g_currenttrack = t;
    
    // Start playing the next track
    sp_session_player_play(g_sess, 1);

    fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Playing track\n");

    sp_link_release(link);
    
    signalTrackStarted(nativeURI);
    
    return 0;
  }
  else
  {
    fprintf(stderr,"jahspotify::Java_jahspotify_impl_JahSpotifyImpl_play: Unable to load link at this point\n");
  }
  
  return 1;
  
}




/**
 * A track has ended. Remove it from the playlist.
 *
 * Called from the main loop when the music_delivery() callback has set g_playback_done.
 */
static void track_ended(void)
{
      fprintf(stderr,"jahspotify::track_ended: called\n");

	int tracks = 0;

	if (g_currenttrack) 
	{
	  sp_link *link = sp_link_create_from_track(g_currenttrack,0);
	  char *trackLinkStr = NULL;
          if (link)
	  {
	        trackLinkStr = malloc ( sizeof ( char ) * ( 100 ) );
		sp_link_as_string(link,trackLinkStr,100);
		sp_link_release(link);
	  }
	  
	  sp_session_player_unload(g_sess);
	  
	  sp_track_release(g_currenttrack);
	  
	  g_currenttrack = NULL;

	  signalTrackEnded(trackLinkStr,JNI_FALSE);
	  
	  if (trackLinkStr)
	  {
	    free(trackLinkStr);
	  }
	}
	
	
}

JNIEXPORT int JNICALL Java_jahspotify_impl_JahSpotifyImpl_initialize ( JNIEnv *env, jobject obj, jstring username, jstring password )
{
    sp_session *sp;
    sp_error err;
    uint8_t *nativePassword = NULL;
    uint8_t *nativeUsername = NULL;
    int next_timeout = 0;

    if ( !username || !password )
    {
        fprintf ( stderr, "Username or password not specified\n" );
        return 1;
    }

    audio_init(&g_audiofifo);

    /* Create session */
    spconfig.application_key_size = g_appkey_size;
    err = sp_session_create ( &spconfig, &sp );

    if ( SP_ERROR_OK != err )
    {
        fprintf ( stderr, "Unable to create session: %s\n",sp_error_message ( err ) );
        return 1;
    }

    fprintf ( stderr, "Session created %x\n",(int)sp);

    nativeUsername = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, username, NULL );
    nativePassword = ( uint8_t * ) ( *env )->GetStringUTFChars ( env, password, NULL );

    g_sess = sp;

    pthread_mutex_init ( &g_notify_mutex, NULL );
    pthread_cond_init ( &g_notify_cond, NULL );

    sp_playlistcontainer_add_callbacks ( sp_session_playlistcontainer ( g_sess ),&pc_callbacks,NULL );

    fprintf ( stderr, "Initiating login: %s\n",nativeUsername );

    sp_session_login ( sp, nativeUsername, nativePassword );
    pthread_mutex_lock ( &g_notify_mutex );


    for ( ;; )
    {
        if ( next_timeout == 0 )
        {
            while ( !g_notify_do && !g_playback_done )
                pthread_cond_wait ( &g_notify_cond, &g_notify_mutex );
        }
        else
        {
            struct timespec ts;

#if _POSIX_TIMERS > 0
            clock_gettime ( CLOCK_REALTIME, &ts );
#else
            struct timeval tv;
            gettimeofday ( &tv, NULL );
            TIMEVAL_TO_TIMESPEC ( &tv, &ts );
#endif
            ts.tv_sec += next_timeout / 1000;
            ts.tv_nsec += ( next_timeout % 1000 ) * 1000000;

            pthread_cond_timedwait ( &g_notify_cond, &g_notify_mutex, &ts );
        }

        g_notify_do = 0;
        pthread_mutex_unlock ( &g_notify_mutex );

        if ( g_playback_done )
        {
            track_ended();
            g_playback_done = 0;
        }

	sp_connectionstate conn_state = sp_session_connectionstate(sp);
	switch (conn_state)
	{
	  case SP_CONNECTION_STATE_UNDEFINED: 
	  case SP_CONNECTION_STATE_LOGGED_OUT:
	  case SP_CONNECTION_STATE_LOGGED_IN:
	    break;
	  case SP_CONNECTION_STATE_DISCONNECTED:
	    fprintf ( stderr, "Disconnected!\n");
	    break;
	}
	

        do
        {
            sp_session_process_events ( sp, &next_timeout );
        }
        while ( next_timeout == 0 );

        pthread_mutex_lock ( &g_notify_mutex );
    }

    // FIXME: Release the username/password allocated?

    return 0;

}