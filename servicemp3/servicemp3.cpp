	/* Note: this requires gstreamer 1.x and a big list of plugins. */
	/* It is currently hardcoded to use a big-endian alsasink as sink. */
#include <lib/base/ebase.h>
#include <lib/base/eerror.h>
#include <lib/base/init_num.h>
#include <lib/base/init.h>
#include <lib/base/nconfig.h>
#include <lib/base/object.h>
#include <lib/dvb/epgcache.h>
#include <lib/dvb/decoder.h>
#include <lib/components/file_eraser.h>
#include <lib/gui/esubtitle.h>
#include <servicemp3.h>
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
#include <servicemp3record.h>
#endif
#include <lib/service/service.h>
#include <lib/gdi/gpixmap.h>

#include <string>

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#endif
#include <sys/stat.h>

#if defined ENABLE_DUAL_MEDIAFW
// || defined ENABLE_LIBEPLAYER3
#include <lib/base/eenv.h>
#endif

#define HTTP_TIMEOUT 60

/*
 * UNUSED variable from service reference is now used as buffer flag for gstreamer
 * REFTYPE:FLAGS:STYPE:SID:TSID:ONID:NS:PARENT_SID:PARENT_TSID:UNUSED
 *   D  D X X X X X X X X
 * 4097:0:1:0:0:0:0:0:0:0:URL:NAME (no buffering)
 * 4097:0:1:0:0:0:0:0:0:1:URL:NAME (buffering enabled)
 * 4097:0:1:0:0:0:0:0:0:3:URL:NAME (progressive download and buffering enabled)
 *
 * Progressive download requires buffering enabled, so it's mandatory to use flag 3 not 2
 */
typedef enum
{
	BUFFERING_ENABLED    = 0x00000001,
	PROGRESSIVE_DOWNLOAD = 0x00000002
} eServiceMP3Flags;

/*
 * GstPlayFlags flags from playbin2. It is the policy of GStreamer to
 * not publicly expose element-specific enums. That is why this
 * GstPlayFlags enum has been copied here.
 */
typedef enum
{
	GST_PLAY_FLAG_VIDEO             = (1 << 0),
	GST_PLAY_FLAG_AUDIO             = (1 << 1),
	GST_PLAY_FLAG_TEXT              = (1 << 2),
	GST_PLAY_FLAG_VIS               = (1 << 3),
	GST_PLAY_FLAG_SOFT_VOLUME       = (1 << 4),
	GST_PLAY_FLAG_NATIVE_AUDIO      = (1 << 5),
	GST_PLAY_FLAG_NATIVE_VIDEO      = (1 << 6),
	GST_PLAY_FLAG_DOWNLOAD          = (1 << 7),
	GST_PLAY_FLAG_BUFFERING         = (1 << 8),
	GST_PLAY_FLAG_DEINTERLACE       = (1 << 9),
	GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
	GST_PLAY_FLAG_FORCE_FILTERS     = (1 << 11),
} GstPlayFlags;

/* static declarations */
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
static GstElement *dvb_audiosink = NULL, *dvb_videosink = NULL, *dvb_subsink = NULL;
#endif

// eServiceFactoryMP3

/*
 * gstreamer suffers from a bug causing sparse streams to loose sync, after pause/resume / skip
 * see: https://bugzilla.gnome.org/show_bug.cgi?id=619434
 * As a workaround, we run the subsink in sync=false mode
 */
#undef GSTREAMER_SUBTITLE_SYNC_MODE_BUG

eServiceFactoryMP3::eServiceFactoryMP3()
{
	ePtr<eServiceCenter> sc;

#if defined ENABLE_DUAL_MEDIAFW
	defaultMP3Player = (::access(eEnv::resolve("${sysconfdir}/enigma2/mp3player").c_str(), F_OK) >= 0);
#endif

	eServiceCenter::getPrivInstance(sc);
	if (sc)
	{
		std::list<std::string> extensions;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
// these audio extensions are handled by this service
// in case of a combined gst/epl3 configuration
		extensions.push_back("dts");  // OK, here
		extensions.push_back("mp2");  // OK, here
		extensions.push_back("mp3");  // OK, here
		extensions.push_back("ogg");  // OK, here
		extensions.push_back("wav");  // OK, here
		extensions.push_back("wave"); // OK, here
		extensions.push_back("flac"); // OK, here
		extensions.push_back("flv");  // OK, here
		extensions.push_back("m4a");  // OK, here
		extensions.push_back("3gp");  // OK, here
		extensions.push_back("3g2");  // OK, here
		extensions.push_back("asf");  // OK, here
		extensions.push_back("wma");  // OK, here
#endif
		extensions.push_back("oga");  // always here
		extensions.push_back("m2a");  // always here
		extensions.push_back("ac3");  // always here
		extensions.push_back("mka");  // always here
		extensions.push_back("aac");  // always here
		extensions.push_back("ape");  // always here
		extensions.push_back("alac"); // always here
#if defined(__sh__)
#if not defined ENABLE_DUAL_MEDIAFW
#if !defined(ENABLE_GSTREAMER) \
 || !defined(ENABLE_LIBEPLAYER3)
/* Hellmaster1024: if both gst and eplayer3 are enabled, this is the GST service!
 * We only select the audio extensions (above), and leave the Video extensions for
 * the ePlayer3 service located in serviceLibpl.
 * If only one of GST and ePlayer3 this service handles all extensions and switches between
 * GST and ePlayer3.
*/
		extensions.push_back("mpg");  // OK, epl
		extensions.push_back("vob");  // OK, epl
		extensions.push_back("m4v");  // OK, epl
		extensions.push_back("mkv");  // OK, epl
		extensions.push_back("avi");  // OK, epl
		extensions.push_back("divx"); // OK, epl
		extensions.push_back("dat");  // OK, epl
		extensions.push_back("mp4");  // OK, epl
		extensions.push_back("mov");  // OK, epl
		extensions.push_back("wmv");  // OK, epl
		extensions.push_back("mpeg"); // OK, epl
		extensions.push_back("mpe");  // OK, epl
		extensions.push_back("rm");   // not in epl!
		extensions.push_back("rmvb"); // not in epl!
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
// these extensions are handled by this service
// in case of a combined gst/epl3 configuration
		extensions.push_back("ogm");  //OK, here
		extensions.push_back("ogv");  //OK, here
#endif
		extensions.push_back("stream"); // not in epl!
		extensions.push_back("webm");  // not in epl!
		extensions.push_back("amr");  // not in epl!
		extensions.push_back("au");   // not in epl!
		extensions.push_back("mid");  // not in epl!
		extensions.push_back("wv");   // not in epl!
		extensions.push_back("pva");  // not in epl!
		extensions.push_back("wtv");  // not in epl!
		extensions.push_back("m2ts"); // OK, epl
		extensions.push_back("trp");  // OK, epl
		extensions.push_back("vdr");  // OK, epl
		extensions.push_back("mts");  // OK, epl
		extensions.push_back("rar");  // OK, epl
		extensions.push_back("img");  // OK, epl
		extensions.push_back("iso");  // OK, epl
		extensions.push_back("ifo");  // OK, epl
//		extensions.push_back("m3u8");
#endif // !GSTREAMER || !LIBEPLAYER3
#endif // DUAL_MEDIAFW
#endif // __sh__
#if defined ENABLE_DUAL_MEDIAFW
		if (defaultMP3Player)
		{
			sc->addServiceFactory(eServiceFactoryMP3::id, this, extensions);
		}
		extensions.clear();
		sc->addServiceFactory(eServiceFactoryMP3::idServiceMP3, this, extensions);
#else
		sc->addServiceFactory(eServiceFactoryMP3::id, this, extensions);
#endif
	}

	m_service_info = new eStaticServiceMP3Info();
}

eServiceFactoryMP3::~eServiceFactoryMP3()
{
	ePtr<eServiceCenter> sc;

	eServiceCenter::getPrivInstance(sc);
	if (sc)
#if defined ENABLE_DUAL_MEDIAFW
	{
		sc->removeServiceFactory(eServiceFactoryMP3::idServiceMP3);
	}
	if (defaultMP3Player)
	{
		sc->removeServiceFactory(eServiceFactoryMP3::id);
	}
#else
	{
		sc->removeServiceFactory(eServiceFactoryMP3::id);
	}
#endif
}

DEFINE_REF(eServiceFactoryMP3)

	// iServiceHandler
RESULT eServiceFactoryMP3::play(const eServiceReference &ref, ePtr<iPlayableService> &ptr)
{
	// check resources...
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!dvb_audiosink && !dvb_videosink && !dvb_subsink)
	{
		// creating gstreamer sinks for the very first media
		eDebug("[eServiceFactoryMP3::%s] first service play", __func__);

		dvb_audiosink = gst_element_factory_make("dvbaudiosink", NULL);
		if (dvb_audiosink)
		{
			gst_object_ref_sink(dvb_audiosink);
			eDebug("[eServiceFactoryMP3::%s] dvb_audiosink created", __func__);
		}

		dvb_videosink = gst_element_factory_make("dvbvideosink", NULL);
		if (dvb_videosink)
		{
			gst_object_ref_sink(dvb_videosink);
			eDebug("[eServiceFactoryMP3::%s] dvb_videosink created", __func__);
		}

		dvb_subsink = gst_element_factory_make("subsink", NULL);
		if (dvb_subsink)
		{
			gst_object_ref_sink(dvb_subsink);
			eDebug("[eServiceFactoryMP3::%s] dvb_subsink created", __func__);
		}
	}
	else
	{
		eDebug("[eServiceFactoryMP3::%s] new service", __func__);
	}
#endif
	ptr = new eServiceMP3(ref);
	return 0;
}

RESULT eServiceFactoryMP3::record(const eServiceReference &ref, ePtr<iRecordableService> &ptr)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (ref.path.find("://") != std::string::npos)
	{
		ptr = new eServiceMP3Record((eServiceReference&)ref);
		return 0;
	}
#endif
	ptr = 0;
	return -1;
}

RESULT eServiceFactoryMP3::list(const eServiceReference &, ePtr<iListableService> &ptr)
{
	ptr = 0;
	return -1;
}

RESULT eServiceFactoryMP3::info(const eServiceReference &ref, ePtr<iStaticServiceInformation> &ptr)
{
	ptr = m_service_info;
	return 0;
}

class eMP3ServiceOfflineOperations: public iServiceOfflineOperations
{
	DECLARE_REF(eMP3ServiceOfflineOperations);
	eServiceReference m_ref;
public:
	eMP3ServiceOfflineOperations(const eServiceReference &ref);

	RESULT deleteFromDisk(int simulate);
	RESULT getListOfFilenames(std::list<std::string> &);
	RESULT reindex();
};

DEFINE_REF(eMP3ServiceOfflineOperations);

eMP3ServiceOfflineOperations::eMP3ServiceOfflineOperations(const eServiceReference &ref): m_ref((const eServiceReference&)ref)
{
}

RESULT eMP3ServiceOfflineOperations::deleteFromDisk(int simulate)
{
	if (!simulate)
	{
		std::list<std::string> res;
		if (getListOfFilenames(res))
		{
			return -1;
		}
		eBackgroundFileEraser *eraser = eBackgroundFileEraser::getInstance();
		if (!eraser)
		{
			eDebug("[eServiceMP3::%s] FATAL! cannot get background file eraser", __func__);
		}
		for (std::list<std::string>::iterator i(res.begin()); i != res.end(); ++i)
		{
			eDebug("[eServiceMP3::%s] Removing %s...", __func__, i->c_str());
			if (eraser)
			{
				eraser->erase(i->c_str());
			}
			else
			{
				::unlink(i->c_str());
			}
		}
	}
	return 0;
}

RESULT eMP3ServiceOfflineOperations::getListOfFilenames(std::list<std::string> &res)
{
	size_t pos;

	res.clear();
	res.push_back(m_ref.path);
	res.push_back(m_ref.path + ".meta");
	res.push_back(m_ref.path + ".cuts");
	std::string filename = m_ref.path;

	if ( (pos = filename.rfind('.')) != std::string::npos)
	{
		filename.erase(pos + 1);
		res.push_back(filename + ".eit");
	}	return 0;
}

RESULT eMP3ServiceOfflineOperations::reindex()
{
	return -1;
}


RESULT eServiceFactoryMP3::offlineOperations(const eServiceReference &ref, ePtr<iServiceOfflineOperations> &ptr)
{
	ptr = new eMP3ServiceOfflineOperations(ref);
	return 0;
}

// eStaticServiceMP3Info

// eStaticServiceMP3Info is separated from eServiceMP3 to give information
// about unopened files.

// Probably eServiceMP3 should use this class as well, and eStaticServiceMP3Info
// should have a database backend where ID3-files etc. are cached.
// This would allow listing the mp3 database based on certain filters.

DEFINE_REF(eStaticServiceMP3Info)

eStaticServiceMP3Info::eStaticServiceMP3Info()
{
}

RESULT eStaticServiceMP3Info::getName(const eServiceReference &ref, std::string &name)
{
	size_t last;

	if (ref.name.length())
	{
		name = ref.name;
	}
	else
	{
		last = ref.path.rfind('/');

		if (last != std::string::npos)
		{
			name = ref.path.substr(last + 1);
		}
		else
		{
			name = ref.path;
		}
	}
	return 0;
}

int eStaticServiceMP3Info::getLength(const eServiceReference &ref)
{
	return -1;
}

int eStaticServiceMP3Info::getInfo(const eServiceReference &ref, int w)
{
	switch (w)
	{
		case iServiceInformation::sTimeCreate:
		{
			struct stat s;

			if (stat(ref.path.c_str(), &s) == 0)
			{
				return s.st_mtime;
			}
			break;
		}
	}
	return iServiceInformation::resNA;
}

long long eStaticServiceMP3Info::getFileSize(const eServiceReference &ref)
{
	struct stat s;
	if (stat(ref.path.c_str(), &s) == 0)
	{
		return s.st_size;
	}
	return 0;
}

RESULT eStaticServiceMP3Info::getEvent(const eServiceReference &ref, ePtr<eServiceEvent> &evt, time_t start_time)
{
	if (ref.path.find("://") != std::string::npos)
	{
		eServiceReference equivalentref(ref);
		equivalentref.type = eServiceFactoryMP3::id;
		equivalentref.path.clear();
		return eEPGCache::getInstance()->lookupEventTime(equivalentref, start_time, evt);
	}
	else // try to read .eit file
	{
		size_t pos;
		ePtr<eServiceEvent> event = new eServiceEvent;
		std::string filename = ref.path;
		if ( (pos = filename.rfind('.')) != std::string::npos)
		{
			filename.erase(pos + 1);
			filename += "eit";
			if (!event->parseFrom(filename, 0))
			{
				evt = event;
				return 0;
			}
		}
	}
	evt = 0;
	return -1;
}

DEFINE_REF(eStreamBufferInfo)

eStreamBufferInfo::eStreamBufferInfo(int percentage, int inputrate, int outputrate, int space, int size)
: bufferPercentage(percentage),
	inputRate(inputrate),
	outputRate(outputrate),
	bufferSpace(space),
	bufferSize(size)
{
}

int eStreamBufferInfo::getBufferPercentage() const
{
	return bufferPercentage;
}

int eStreamBufferInfo::getAverageInputRate() const
{
	return inputRate;
}

int eStreamBufferInfo::getAverageOutputRate() const
{
	return outputRate;
}

int eStreamBufferInfo::getBufferSpace() const
{
	return bufferSpace;
}

int eStreamBufferInfo::getBufferSize() const
{
	return bufferSize;
}

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
DEFINE_REF(eServiceMP3InfoContainer);

eServiceMP3InfoContainer::eServiceMP3InfoContainer()
: doubleValue(0.0), bufferValue(NULL), bufferData(NULL), bufferSize(0)
{
}

eServiceMP3InfoContainer::~eServiceMP3InfoContainer()
{
	if (bufferValue)
	{
		gst_buffer_unmap(bufferValue, &map);
		gst_buffer_unref(bufferValue);
		bufferValue = NULL;
		bufferData = NULL;
		bufferSize = 0;
	}
}

double eServiceMP3InfoContainer::getDouble(unsigned int index) const
{
	return doubleValue;
}

unsigned char *eServiceMP3InfoContainer::getBuffer(unsigned int &size) const
{
	size = bufferSize;
	return bufferData;
}

void eServiceMP3InfoContainer::setDouble(double value)
{
	doubleValue = value;
}

void eServiceMP3InfoContainer::setBuffer(GstBuffer *buffer)
{
	bufferValue = buffer;
	gst_buffer_ref(bufferValue);
	gst_buffer_map(bufferValue, &map, GST_MAP_READ);
	bufferData = map.data;
	bufferSize = map.size;
}
#endif

// eServiceMP3
int eServiceMP3::ac3_delay = 0,
    eServiceMP3::pcm_delay = 0;

eServiceMP3::eServiceMP3(eServiceReference ref):
	m_nownext_timer(eTimer::create(eApp)),
	m_cuesheet_changed(0),
	m_cutlist_enabled(1),
	m_ref(ref),
	m_pump(eApp, 1)
{
	eDebug("[eServiceMP3] %s >", __func__);
	m_subtitle_sync_timer = eTimer::create(eApp);
	m_streamingsrc_timeout = 0;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	m_stream_tags = 0;
#endif
	m_currentAudioStream = -1;
	m_currentSubtitleStream = -1;
	m_cachedSubtitleStream = -2; /* report subtitle stream to be 'cached'. TODO: use an actual cache. */
	m_subtitle_widget = 0;
	m_currentTrickRatio = 1.0;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	m_buffer_size = 5LL * 1024LL * 1024LL;
#else
	m_buffer_size = 8LL * 1024LL * 1024LL;
#endif
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	m_ignore_buffering_messages = 0;
	m_is_live = false;
	m_use_prefillbuffer = false;
	m_paused = false;
	m_first_paused = false;
	m_autoaudio = true;
	m_cuesheet_loaded = false; /* cuesheet CVR */
	m_audiosink_not_running = false;
	m_use_chapter_entries = false; /* TOC chapter support CVR */
	m_last_seek_pos = 0; /* CVR last seek position */
	m_play_position_timer = eTimer::create(eApp);
	CONNECT(m_play_position_timer->timeout, eServiceMP3::playPositionTiming);
	m_use_last_seek = false;
	m_useragent = "Enigma2 HbbTV/1.1.1 (+PVR+RTSP+DL;OpenPLi;;;)";
	m_extra_headers = "";
	m_download_buffer_path = "";
#endif
	m_prev_decoder_time = -1;
	m_decoder_time_valid_state = 0;
	m_errorInfo.missing_codec = "";
	m_subs_to_pull_handler_id = m_notify_source_handler_id = m_notify_element_added_handler_id = 0;
	m_decoder = NULL;

	CONNECT(m_subtitle_sync_timer->timeout, eServiceMP3::pushSubtitles);
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	CONNECT(m_pump.recv_msg, eServiceMP3::gstPoll);
#endif
	CONNECT(m_nownext_timer->timeout, eServiceMP3::updateEpgCacheNowNext);
	m_aspect = m_width = m_height = m_framerate = m_progressive = m_gamma = -1;

	m_state = stIdle;
	m_gstdot = eConfigManager::getConfigBoolValue("config.crash.gstdot");
	m_subtitles_paused = false;
	m_coverart = false;
	eDebug("[eServiceMP3::%s] construct", __func__);

	const char *filename;
	std::string filename_str;
	size_t pos = m_ref.path.find('#');
	if (pos != std::string::npos && (m_ref.path.compare(0, 4, "http") == 0 || m_ref.path.compare(0, 4, "rtsp") == 0))
	{
		filename_str = m_ref.path.substr(0, pos);
		filename = filename_str.c_str();
		m_extra_headers = m_ref.path.substr(pos + 1);

		pos = m_extra_headers.find("User-Agent=");
		if (pos != std::string::npos)
		{
			size_t hpos_start = pos + 11;
			size_t hpos_end = m_extra_headers.find('&', hpos_start);
			if (hpos_end != std::string::npos)
			{
				m_useragent = m_extra_headers.substr(hpos_start, hpos_end - hpos_start);
			}
			else
			{
				m_useragent = m_extra_headers.substr(hpos_start);
			}
		}
	}
	else
	{
		filename = m_ref.path.c_str();
	}
	const char *ext = strrchr(filename, '.');
	if (!ext)
	{
		ext = filename + strlen(filename);
	}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	m_sourceinfo.audiotype = atUnknown;
	if (strcasecmp(ext, ".mpeg") == 0
	||  strcasecmp(ext, ".mpe")  == 0
	||  strcasecmp(ext, ".mpg")  == 0
	||  strcasecmp(ext, ".vob")  == 0
	||  strcasecmp(ext, ".bin")  == 0)
	{
		m_sourceinfo.containertype = ctMPEGPS;
	}
	else if (strcasecmp(ext, ".ts") == 0)
	{
		m_sourceinfo.containertype = ctMPEGTS;
	}
	else if (strcasecmp(ext, ".mkv") == 0)
	{
		m_sourceinfo.containertype = ctMKV;
	}
	else if (strcasecmp(ext, ".ogm") == 0
	||       strcasecmp(ext, ".ogv") == 0)
	{
		m_sourceinfo.containertype = ctOGG;
	}
	else if (strcasecmp(ext, ".avi") == 0
	||       strcasecmp(ext, ".divx") == 0)
	{
		m_sourceinfo.containertype = ctAVI;
	}
	else if (strcasecmp(ext, ".mp4") == 0
	||       strcasecmp(ext, ".mov") == 0
	||       strcasecmp(ext, ".m4v") == 0
	||       strcasecmp(ext, ".3gp") == 0
	||       strcasecmp(ext, ".3g2") == 0)
	{
		m_sourceinfo.containertype = ctMP4;
	}
	else if (strcasecmp(ext, ".asf") == 0
	||       strcasecmp(ext, ".wmv") == 0)
	{
		m_sourceinfo.containertype = ctASF;
	}
	else if (strcasecmp(ext, ".webm") == 0)
	{
		m_sourceinfo.containertype = ctMKV;
	}
	else if (strcasecmp(ext, ".m4a") == 0
	||       strcasecmp(ext, ".alac") == 0)
	{
		m_sourceinfo.containertype = ctMP4;
		m_sourceinfo.audiotype = atAAC;
	}
	else if (strcasecmp(ext, ".m3u8") != 0)
	{
		m_sourceinfo.is_hls = TRUE;
	}
	else if (strcasecmp(ext, ".mp3") == 0)
	{
		m_sourceinfo.audiotype = atMP3;
		m_sourceinfo.is_audio = TRUE;
	}
	else if (strcasecmp(ext, ".wma") == 0)
	{
		m_sourceinfo.audiotype = atWMA;
		m_sourceinfo.is_audio = TRUE;
	}
	else if (strcasecmp(ext, ".wav") == 0
	||       strcasecmp(ext, ".wave") == 0
	||       strcasecmp(ext, ".wv") == 0)
	{
		m_sourceinfo.audiotype = atPCM;
		m_sourceinfo.is_audio = TRUE;
	}
	else if (strcasecmp(ext, ".dts") == 0)
	{
		m_sourceinfo.audiotype = atDTS;
		m_sourceinfo.is_audio = TRUE;
	}
	else if (strcasecmp(ext, ".flac") == 0)
	{
		m_sourceinfo.audiotype = atFLAC;
		m_sourceinfo.is_audio = TRUE;
	}
	else if (strcasecmp(ext, ".ac3") == 0)
	{
		m_sourceinfo.audiotype = atAC3;
		m_sourceinfo.is_audio = TRUE;
	}
	else if (strcasecmp(ext, ".cda") == 0)
	{
		m_sourceinfo.containertype = ctCDA;
	}
	if (strcasecmp(ext, ".dat") == 0)
	{
		m_sourceinfo.containertype = ctVCD;
	}
	if (strstr(filename, "://"))
	{
		m_sourceinfo.is_streaming = TRUE;
	}
	gchar *uri;
	gchar *suburi = NULL;

	pos = m_ref.path.find("&suburi=");
	if (pos != std::string::npos)
	{
		filename_str = filename;

		std::string suburi_str = filename_str.substr(pos + 8);
		filename = suburi_str.c_str();
		suburi = g_strdup_printf ("%s", filename);

		filename_str = filename_str.substr(0, pos);
		filename = filename_str.c_str();
	}

	if (m_sourceinfo.is_streaming)
	{
		if (eConfigManager::getConfigBoolValue("config.mediaplayer.useAlternateUserAgent"))
		{
			m_useragent = eConfigManager::getConfigValue("config.mediaplayer.alternateUserAgent");
		}
		uri = g_strdup_printf ("%s", filename);
		m_streamingsrc_timeout = eTimer::create(eApp);;
		CONNECT(m_streamingsrc_timeout->timeout, eServiceMP3::sourceTimeout);

		if (m_ref.getData(7) & BUFFERING_ENABLED)
		{
			if (m_ref.getData(7) & PROGRESSIVE_DOWNLOAD)
			{
				/* progressive download buffering */
				if (::access("/hdd/movie", X_OK) >= 0)
				{
					/* It looks like /hdd points to a valid mount, so we can store a download buffer on it */
					m_download_buffer_path = "/hdd/gstreamer_XXXXXXXXXX";
				}
			}
		}
	}
	else if (m_sourceinfo.containertype == ctCDA)
	{
		int i_track = atoi(filename + (strlen(filename) - 6));
		uri = g_strdup_printf ("cdda://%i", i_track);
	}
	else if (m_sourceinfo.containertype == ctVCD)
	{
		int ret = -1;
		int fd = open(filename,O_RDONLY);
		if (fd >= 0)
		{
			char *tmp = new char[128 * 1024];
			ret = read(fd, tmp, 128 * 1024);
			close(fd);
			delete [] tmp;
		}
		if (ret == -1) // this is a "REAL" VCD
		{
			uri = g_strdup_printf ("vcd://");
		}
		else
		{
			uri = g_filename_to_uri(filename, NULL, NULL);
		}
	}
	else
	{
		uri = g_filename_to_uri(filename, NULL, NULL);
	}
	eDebug("[eServiceMP3] playbin uri=%s", uri);
	if (suburi != NULL)
	{
		eDebug("[eServiceMP3] playbin suburi=%s", suburi);
	}
	m_gst_playbin = gst_element_factory_make("playbin", "playbin");

	if (m_gst_playbin)
	{
		if (dvb_audiosink)
		{
			if (m_sourceinfo.is_audio)
			{
				g_object_set(dvb_audiosink, "e2-sync", TRUE, NULL);
				g_object_set(dvb_audiosink, "e2-async", TRUE, NULL);
			}
			else
			{
				g_object_set(dvb_audiosink, "e2-sync", FALSE, NULL);
				g_object_set(dvb_audiosink, "e2-async", FALSE, NULL);
			}
			g_object_set(m_gst_playbin, "audio-sink", dvb_audiosink, NULL);
		}
		if (dvb_videosink && !m_sourceinfo.is_audio)
		{
			g_object_set(dvb_videosink, "e2-sync", FALSE, NULL);
			g_object_set(dvb_videosink, "e2-async", FALSE, NULL);
			g_object_set(m_gst_playbin, "video-sink", dvb_videosink, NULL);
		}
		/*
		 * avoid video conversion, let the dvbmediasink handle that using native video flag
		 * volume control is done by hardware, do not use soft volume flag
		 */
		guint flags = GST_PLAY_FLAG_AUDIO
		            | GST_PLAY_FLAG_VIDEO
		            | GST_PLAY_FLAG_TEXT
		            | GST_PLAY_FLAG_NATIVE_VIDEO;

		if (m_sourceinfo.is_streaming)
		{
			m_notify_source_handler_id = g_signal_connect(m_gst_playbin, "notify::source", G_CALLBACK(playbinNotifySource), this);
			if (m_download_buffer_path != "")
			{
				/* use progressive download buffering */
				flags |= GST_PLAY_FLAG_DOWNLOAD;
				m_notify_element_added_handler_id = g_signal_connect(m_gst_playbin, "element-added", G_CALLBACK(handleElementAdded), this);
				/* limit file size */
				g_object_set(m_gst_playbin, "ring-buffer-max-size", (guint64)(8LL * 1024LL * 1024LL), NULL);
			}
			/*
			 * regardless whether or not we configured a progressive download file, use a buffer as well
			 * (progressive download might not work for all formats)
			 */
			flags |= GST_PLAY_FLAG_BUFFERING;
			/* increase the default 2 second / 2 MB buffer limitations to 10s / 10MB */
			g_object_set(m_gst_playbin, "buffer-duration", (gint64)(5LL * GST_SECOND), NULL);
			g_object_set(m_gst_playbin, "buffer-size", m_buffer_size, NULL);
			if (m_sourceinfo.is_hls)
			{
				g_object_set(m_gst_playbin, "connection-speed", (guint64)(4495000LL), NULL);
			}
			/* set network connection speed from config */
			int bitrate = eConfigManager::getConfigIntValue("config.streaming.connectionSpeedInKb");
			g_object_set(G_OBJECT(m_gst_playbin), "connection-speed", (guint64)bitrate, NULL);
		}
		g_object_set(m_gst_playbin, "flags", flags, NULL);
		g_object_set(m_gst_playbin, "uri", uri, NULL);
		if (dvb_subsink)
		{
			m_subs_to_pull_handler_id = g_signal_connect(dvb_subsink, "new-buffer", G_CALLBACK(gstCBsubtitleAvail), this);
			g_object_set(dvb_subsink, "caps", gst_caps_from_string("text/plain; text/x-plain; text/x-raw; text/x-pango-markup; subpicture/x-dvd; subpicture/x-pgs"), NULL);
			g_object_set(m_gst_playbin, "text-sink", dvb_subsink, NULL);
			g_object_set(m_gst_playbin, "current-text", m_currentSubtitleStream, NULL);
		}
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_gst_playbin));
		gst_bus_set_sync_handler(bus, gstBusSyncHandler, this, NULL);
		gst_object_unref(bus);

		if (suburi != NULL)
		{
			g_object_set(G_OBJECT(m_gst_playbin), "suburi", suburi, NULL);
		}
		else
		{
			char srt_filename[ext - filename + 5];
			sprintf(srt_filename, "%.*s.vtt", ext - filename, filename);
			if (::access(srt_filename, R_OK) < 0)
			{
				strcpy(srt_filename + (ext - filename), ".srt");
			}
			if (::access(srt_filename, R_OK) >= 0)
			{
				gchar *luri = g_filename_to_uri(srt_filename, NULL, NULL);
				eDebug("[eServiceMP3] subtitle uri: %s", luri);
				g_object_set(m_gst_playbin, "suburi", luri, NULL);
				g_free(luri);
			}
		}
	}
	else
	{
		m_event((iPlayableService*)this, evUser+12);
		m_gst_playbin = NULL;
		m_errorInfo.error_message = "failed to create GStreamer pipeline!\n";

		eDebug("[eServiceMP3] sorry, cannot play: %s",m_errorInfo.error_message.c_str());
	}
	g_free(uri);
	if (suburi != NULL)
	{
		g_free(suburi);
	}
#else // eplayer3
	instance = this;

	player = new Player();

	//create playback path
	char file[1023] = {""};
	if ((!strncmp("http://", m_ref.path.c_str(), 7))
	|| (!strncmp("https://", m_ref.path.c_str(), 8))
	|| (!strncmp("cache://", m_ref.path.c_str(), 8))
	|| (!strncmp("concat://", m_ref.path.c_str(), 9))
	|| (!strncmp("crypto://", m_ref.path.c_str(), 9))
	|| (!strncmp("gopher://", m_ref.path.c_str(), 9))
	|| (!strncmp("hls://", m_ref.path.c_str(), 6))
	|| (!strncmp("hls+http://", m_ref.path.c_str(), 11))
	|| (!strncmp("httpproxy://", m_ref.path.c_str(), 12))
	|| (!strncmp("mms://", m_ref.path.c_str(), 6))
	|| (!strncmp("mmsh://", m_ref.path.c_str(), 7))
	|| (!strncmp("mmst://", m_ref.path.c_str(), 7))
	|| (!strncmp("rtmp://", m_ref.path.c_str(), 7))
	|| (!strncmp("rtmpe://", m_ref.path.c_str(), 8))
	|| (!strncmp("rtmpt://", m_ref.path.c_str(), 8))
	|| (!strncmp("rtmps://", m_ref.path.c_str(), 8))
	|| (!strncmp("rtmpte://", m_ref.path.c_str(), 9))
	|| (!strncmp("ftp://", m_ref.path.c_str(), 6))
	|| (!strncmp("rtp://", m_ref.path.c_str(), 6))
	|| (!strncmp("srtp://", m_ref.path.c_str(), 7))
	|| (!strncmp("subfile://", m_ref.path.c_str(), 10))
	|| (!strncmp("tcp://", m_ref.path.c_str(), 6))
	|| (!strncmp("tls://", m_ref.path.c_str(), 6))
	|| (!strncmp("udp://", m_ref.path.c_str(), 6))
	|| (!strncmp("udplite://", m_ref.path.c_str(), 10)))
	{
		is_streaming = true;
	}
	else if ((!strncmp("file://", m_ref.path.c_str(), 7))
	||       (!strncmp("bluray://", m_ref.path.c_str(), 9))
	||       (!strncmp("hls+file://", m_ref.path.c_str(), 11))
	||       (!strncmp("myts://", m_ref.path.c_str(), 7)))
	{
		is_streaming = false;
	}
	else
	{
		strcat(file, "file://");
	}
	// try parse HLS master playlist to use streams from it
	size_t delim_idx = m_ref.path.rfind(".");
	if (!strncmp("http", m_ref.path.c_str(), 4) && delim_idx != std::string::npos && !m_ref.path.compare(delim_idx, 5, ".m3u8"))
	{
		M3U8VariantsExplorer ve(m_ref.path);
		std::vector<M3U8StreamInfo> m_stream_vec = ve.getStreams();
		if (m_stream_vec.empty())
		{
			eDebug("[eServiceMP3::%s] failed to retrieve m3u8 streams", __func__);
			strcat(file, m_ref.path.c_str());
		}
		else
		{
			// sort streams from best quality to worst (internally sorted according to bitrate)
			sort(m_stream_vec.rbegin(), m_stream_vec.rend());
			unsigned int bitrate = eConfigManager::getConfigIntValue("config.streaming.connectionSpeedInKb") * 1000L;
			std::vector<M3U8StreamInfo>::const_iterator it(m_stream_vec.begin());
			while(!(it == m_stream_vec.end() || it->bitrate <= bitrate))
			{
				it++;
			}
			eDebug("[eServiceMP3::%s] play stream (%lu b/s) selected according to connection speed (%d b/s)",
				__func__, it->bitrate, bitrate);
			strcat(file, it->url.c_str());
		}
	}
	else
	{
		strcat(file, m_ref.path.c_str());
	}
	// Try to open file
	if (player->Open(file, is_streaming, ""))
	{
		eDebug("[eServiceMP3::%s] Open file", __func__);

		std::vector<Track> tracks = player->getAudioTracks();
		if (!tracks.empty())
		{
			eDebug("[eServiceMP3::%s] Audio track list:", __func__);
			for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end(); ++it) 
			{
				eDebug("[eServiceMP3::%s]    Id:%i type:%i language:%s", __func__, it->pid, it->type, it->title.c_str());
				audioStream audio;
				audio.language_code = it->title;
				audio.pid = it->pid;
				audio.type = it->type;

				m_audioStreams.push_back(audio);
			}
			m_currentAudioStream = 0;
		}

		tracks = player->getSubtitleTracks();
		if (!tracks.empty())
		{
			eDebug("[eServiceMP3::%s] Subtitle track list:", __func__);
			for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end(); ++it) 
			{
				eDebug("[eServiceMP3::%s]    Id:%i type:%i language:%s", __func__, it->pid, it->type, it->title.c_str());
				subtitleStream subtitle;
				subtitle.language_code = it->title;
				subtitle.id = it->pid;
				subtitle.type = it->type;

				m_subtitleStreams.push_back(subtitle);
			}
		}

		loadCuesheet(); /* cuesheet CVR */

		if (!strncmp(file, "file://", 7)) /* text subtitles */
		{
			ReadTextSubtitles(file);
		}
	}
	else
	{
		// Creation failed, no playback support for insert file, so send e2 EOF to stop playback
		eDebug("[eServiceMP3::%s] ERROR: Creation failed! No playback support for insert file!", __func__);
		m_state = stStopped;
		m_event((iPlayableService*)this, evEOF);
		m_event((iPlayableService*)this, evUser+12);
	}
#endif
}

eServiceMP3::~eServiceMP3()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	// disconnect subtitle callback
	if (dvb_subsink)
	{
		g_signal_handler_disconnect(dvb_subsink, m_subs_to_pull_handler_id);
		if (m_subtitle_widget)
		{
			disableSubtitles();
		}
	}
#endif

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (m_gst_playbin)
	{
		if (m_notify_source_handler_id)
		{
			g_signal_handler_disconnect(m_gst_playbin, m_notify_source_handler_id);
			m_notify_source_handler_id = 0;
		}
		if (m_notify_element_added_handler_id)
		{
			g_signal_handler_disconnect(m_gst_playbin, m_notify_element_added_handler_id);
			m_notify_element_added_handler_id = 0;
		}
		// disconnect sync handler callback
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_gst_playbin));
		gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
		gst_object_unref(bus);
	}
#endif
	stop();

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (m_decoder)
	{
		m_decoder = NULL;
	}
	if (m_stream_tags)
	{
		gst_tag_list_free(m_stream_tags);
	}
	if (m_gst_playbin)
	{
		gst_object_unref(GST_OBJECT(m_gst_playbin));
		m_ref.path.clear();
		m_ref.name.clear();
		m_play_position_timer->stop();
		m_last_seek_pos = 0;
		m_use_last_seek = false;
		eDebug("[eServiceMP3::%s] pipeline destructed", __func__);
	}
#endif
}

void eServiceMP3::updateEpgCacheNowNext()
{
	bool update = false;
	ePtr<eServiceEvent> next = 0;
	ePtr<eServiceEvent> ptr = 0;
	eServiceReference ref(m_ref);
	ref.type = eServiceFactoryMP3::id;
	ref.path.clear();
	if (eEPGCache::getInstance() && eEPGCache::getInstance()->lookupEventTime(ref, -1, ptr) >= 0)
	{
		ePtr<eServiceEvent> current = m_event_now;
		if (!current || !ptr || current->getEventId() != ptr->getEventId())
		{
			update = true;
			m_event_now = ptr;
			time_t next_time = ptr->getBeginTime() + ptr->getDuration();
			if (eEPGCache::getInstance()->lookupEventTime(ref, next_time, ptr) >= 0)
			{
				next = ptr;
				m_event_next = ptr;
			}
		}
	}

	int refreshtime = 60;
	if (!next)
	{
		next = m_event_next;
	}
	if (next)
	{
		time_t now = eDVBLocalTimeHandler::getInstance()->nowTime();
		refreshtime = (int)(next->getBeginTime() - now) + 3;
		if (refreshtime <= 0 || refreshtime > 60)
		{
			refreshtime = 60;
		}
	}
	m_nownext_timer->startLongTimer(refreshtime);
	if (update)
	{
		m_event((iPlayableService*)this, evUpdatedEventInfo);
	}
}

DEFINE_REF(eServiceMP3);

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
DEFINE_REF(GstMessageContainer);
#endif

RESULT eServiceMP3::connectEvent(const sigc::slot2<void,iPlayableService*,int> &event, ePtr<eConnection> &connection)
{
	connection = new eConnection((iPlayableService*)this, m_event.connect(event));
#if !defined ENABLE_GSTREAMER \
 && !defined ENABLE_DUAL_MEDIAFW
	m_event(this, evSeekableStatusChanged);
#endif
	return 0;
}

RESULT eServiceMP3::start()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	ASSERT(m_state == stIdle);
#else // eplayer3
	if (m_state != stIdle)
	{
		eDebug("[eServiceMP3::%s] < m_state != stIdle", __func__);
		return -1;
	}
#endif

	m_subtitles_paused = false;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (m_gst_playbin)
	{
		eDebug("[eServiceMP3] starting pipeline");
		GstStateChangeReturn ret;
		ret = gst_element_set_state(m_gst_playbin, GST_STATE_READY);

		switch(ret)
		{
			case GST_STATE_CHANGE_FAILURE:
			{
				eDebug("[eServiceMP3] failed to start pipeline");
				stop();
				return -1;
			}
			case GST_STATE_CHANGE_SUCCESS:
			{
				m_is_live = false;
				break;
			}
			case GST_STATE_CHANGE_NO_PREROLL:
			{
				gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
				m_is_live = true;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	if (m_ref.path.find("://") == std::string::npos)
	{
		/* read event from .eit file */
		size_t pos;
		ePtr<eServiceEvent> event = new eServiceEvent;
		std::string filename = m_ref.path;
		if ((pos = filename.rfind('.')) != std::string::npos)
		{
			filename.erase(pos + 1);
			filename += "eit";
			if (!event->parseFrom(filename, 0))
			{
				ePtr<eServiceEvent> empty;
				m_event_now = event;
				m_event_next = empty;
			}
		}
	}

	return 0;
#else // eplayer3
	if (m_state != stIdle)
	{
		eDebug("[eServiceMP3::%s] state is not idle", __func__);
		return -1;
	}
	if (player && player->Play())
	{
		m_state = stRunning;

		int autoaudio = 0;
		int autoaudio_level = 5;
		std::string configvalue;
		std::vector<std::string> autoaudio_languages;
		configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect1");
		if (configvalue != "" && configvalue != "None")
		{
			autoaudio_languages.push_back(configvalue);
		}
		configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect2");
		if (configvalue != "" && configvalue != "None")
		{
			autoaudio_languages.push_back(configvalue);
		}
		configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect3");
		if (configvalue != "" && configvalue != "None")
		{
			autoaudio_languages.push_back(configvalue);
		}
		configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect4");
		if (configvalue != "" && configvalue != "None")
		{
			autoaudio_languages.push_back(configvalue);
		}
		for (unsigned int i = 0; i < m_audioStreams.size(); i++)
		{
			if (!m_audioStreams[i].language_code.empty())
			{
				int x = 1;
				for (std::vector<std::string>::iterator it = autoaudio_languages.begin(); x < autoaudio_level && it != autoaudio_languages.end(); x++, it++)
				{
					if ((*it).find(m_audioStreams[i].language_code) != std::string::npos)
					{
						autoaudio = i;
						autoaudio_level = x;
						break;
					}
				}
			}
		}

		if (autoaudio)
		{
			selectAudioStream(autoaudio);
		}
		m_event(this, evStart);
		m_event(this, evGstreamerPlayStarted);
		updateEpgCacheNowNext();
		eDebug("[eServiceMP3::%s] < %s", __func__, m_ref.path.c_str());
		return 0;
	}
	eDebug("[eServiceMP3::%s] ERROR starting %s", __func__, m_ref.path.c_str());
	return -1;
#endif
}

void eServiceMP3::sourceTimeout()
{
	eDebug("[eServiceMP3] http source timeout! issuing eof...");
	stop();
	m_event((iPlayableService*)this, evEOF);
}

RESULT eServiceMP3::stop()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_gst_playbin || m_state == stStopped)
#else
	if (m_state == stIdle)
#endif
	{
		// eDebug("[eServiceMP3::%s] < m_state == stIdle", __func__);
		return -1;
	}
	eDebug("[eServiceMP3::%s] stop %s", __func__, m_ref.path.c_str());
	m_state = stStopped;
	m_subtitles_paused = false;

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	GstStateChangeReturn ret;
	GstState state, pending;
	/* make sure that last state change was successful */
	ret = gst_element_get_state(m_gst_playbin, &state, &pending, 5 * GST_SECOND);
	eDebug("[eServiceMP3::%s] stop state:%s pending:%s ret:%s", __func__,
		gst_element_state_get_name(state),
		gst_element_state_get_name(pending),
		gst_element_state_change_return_get_name(ret));

	ret = gst_element_set_state(m_gst_playbin, GST_STATE_NULL);
	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		eDebug("[eServiceMP3::%s] stop GST_STATE_NULL failure", __func__);
	}
	if (!m_sourceinfo.is_streaming && m_cuesheet_loaded)
	{
		saveCuesheet();
	}
	m_nownext_timer->stop();
	if (m_streamingsrc_timeout)
	{
		m_streamingsrc_timeout->stop();
	}
	/* make sure that media is stopped before proceeding further */
	ret = gst_element_get_state(m_gst_playbin, &state, &pending, 5 * GST_SECOND);
	eDebug("[eServiceMP3::%s] to NULL state:%s pending:%s ret:%s", __func__,
		gst_element_state_get_name(state),
		gst_element_state_get_name(pending),
		gst_element_state_change_return_get_name(ret));

#else // eplayer3
	if (m_state == stStopped)
	{
		eDebug("[eServiceMP3::%s] state is stopped", __func__);
		return -1;
	}
	eDebug("[eServiceMP3::%s] stop %s", __func__, m_ref.path.c_str());

	player->RequestAbort();
	player->Stop();
	player->Close();

	m_state = stStopped;
	m_subtitles_paused = false;
	saveCuesheet();
	m_nownext_timer->stop();
	if (m_streamingsrc_timeout)
	{
		m_streamingsrc_timeout->stop();
	}
#endif
	return 0;
}

void eServiceMP3::playPositionTiming()
{
	//eDebug("[eServiceMP3] ***** USE IOCTL POSITION ******");
	m_use_last_seek = false;
}

RESULT eServiceMP3::pause(ePtr<iPauseableService> &ptr)
{
	ptr = this;
	return 0;
}

#if !defined ENABLE_GSTREAMER \
 && !defined ENABLE_DUAL_MEDIAFW
// eplayer3
int speed_mapping[] =
{
 /* e2_ratio   speed */
	2,         1,
	4,         3,
	8,         7,
	16,        15,
	32,        31,
	64,        63,
	128,      127,
	-2,       -5,
	-4,      -10,
	-8,      -20,
	-16,      -40,
	-32,      -80,
	-64,     -160,
	-128,     -320,
	-1,       -1
};

int getSpeed(int ratio)
{
	int i = 0;
	while (speed_mapping[i] != -1)
	{
		if (speed_mapping[i] == ratio)
		{
			return speed_mapping[i + 1];
		}
		i += 2;
	}
	return -1;
}
#endif

RESULT eServiceMP3::setSlowMotion(int ratio)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!ratio)
	{
		return 0;
	}
	eDebug("[eServiceMP3::%s] ratio=%.1f", __func__, 1.0 / (gdouble)ratio);
	return trickSeek(1.0 / (gdouble)ratio);
#else // eplayer3
	// konfetti: in libeplayer3 we changed this because I do not like application specific stuff in a library
	int speed = getSpeed(ratio);

	if (m_state == stRunning && speed != -1 && ratio > 1)
	{
		if (player->SlowMotion(speed))
		{
			eDebug("[eServiceMP3::%s] ERROR!", __func__);
			return -1;
		}
	}
	return 0;
#endif
}

RESULT eServiceMP3::setFastForward(int ratio)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	eDebug("[eServiceMP3::%s] ratio=%.1f", __func__, (gdouble)ratio);
	return trickSeek(ratio);
#else // eplayer3
// konfetti: in libeplayer3 we changed this because I do not like application specific stuff in a library
	int speed = getSpeed(ratio);

	int res = 0;

	eDebug("[eServiceMP3::%s] ratio=%.1f", __func__, (gdouble)ratio);
	if (m_state == stRunning && speed != -1)
	{
		if (ratio > 1)
		{
			res = player->FastForward(speed);
		}
		else if (ratio < -1)
		{
			//speed = speed * -1;
			res = player->FastBackward(speed);
		}
		else /* speed == 1 */
		{
			res = player->Continue();
		}
		if (res)
		{
			eDebug("[eServiceMP3::%s] ERROR!", __func__);
			return -1; //missing?
		}
	}
	return 0;
#endif
}

	// iPausableService
RESULT eServiceMP3::pause()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_gst_playbin || m_state != stRunning)
	{
		return -1;
	}
	eDebug("[eServiceMP3::%s] >", __func__);
	m_subtitles_paused = true;
	m_subtitle_sync_timer->start(1, true);
	if (!m_paused)
	{
		trickSeek(0.0);
	}
#else // eplayer3
	if (m_state != stRunning)
	{
		return -1;
	}
	eDebug("[eServiceMP3::%s] >", __func__);
	m_subtitles_paused = true;
	m_subtitle_sync_timer->start(1, true);
	if (!m_paused)
	{
		player->Pause();
	}
#endif
	else
	{
		eDebug("[eServiceMP3::%s] Already paused; no need to pause", __func__);
	}
	return 0;
}

RESULT eServiceMP3::unpause()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_gst_playbin || m_state != stRunning)
	{
		return -1;
	}
	m_subtitles_paused = false;
	m_subtitle_sync_timer->start(1, true);
	m_decoder_time_valid_state = 0;
	/* no need to unpause if we are not paused already */
	if (m_currentTrickRatio == 1.0 && !m_paused)
	{
		eDebug("[eServiceMP3::%s] trickSeek; no need to unpause!", __func__);
		return 0;
	}

	eDebug("[eServiceMP3::%s] <", __func__);
	trickSeek(1.0);
#else  // eplayer3
	if (m_state != stRunning)
	{
		return -1;
	}
	m_decoder_time_valid_state = 0;
	m_subtitles_paused = false;
	m_subtitle_sync_timer->start(1, true);
	/* no need to unpause if we are not paused already */
	if (m_currentTrickRatio == 1.0 && !m_paused)
	{
		eDebug("[eServiceMP3::%s] already playing; no need to unpause!", __func__);
		return 0;
	}
	eDebug("[eServiceMP3::%s] <", __func__);
	player->Continue();
#endif
	return 0;
}

	/* iSeekableService */
RESULT eServiceMP3::seek(ePtr<iSeekableService> &ptr)
{
	ptr = this;
	return 0;
}

RESULT eServiceMP3::getLength(pts_t &pts)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_gst_playbin || m_state != stRunning)
	{
		return -1;
	}
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 len;
	if (!gst_element_query_duration(m_gst_playbin, fmt, &len))
	{
		return -1;
	}
	/* len is in nanoseconds. There are 90.000 pts per second. */
	pts = len / 11111LL;
#else // eplayer3
	if (m_state != stRunning)
	{
		return -1;
	}
	int64_t length = 0;
	player->GetDuration(length);
	if (length <= 0)
	{
		return -1;
	}
	pts = length * 90000;
#endif
	return 0;
}

RESULT eServiceMP3::seekToImpl(pts_t to)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	m_last_seek_pos = to;
	if (!gst_element_seek(m_gst_playbin, m_currentTrickRatio, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
		GST_SEEK_TYPE_SET, (gint64)(m_last_seek_pos * 11111LL),
		GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
	{
		eDebug("[eServiceMP3::%s] failed", __func__);
		return -1;
	}

	if (m_paused)
	{
		m_event((iPlayableService*)this, evUpdatedInfo);
	}
#endif
	return 0;
}

RESULT eServiceMP3::seekTo(pts_t to)
{
	RESULT ret = -1;

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (m_gst_playbin)
	{
		m_prev_decoder_time = -1;
		m_decoder_time_valid_state = 0;
		ret = seekToImpl(to);
	}
#else // eplayer3
	if (m_state != stRunning)
	{
		return -1;
	}
	player->Seek((int64_t)to / 90000, true);
	ret = 0;
#endif
	return ret;
}

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
RESULT eServiceMP3::trickSeek(gdouble ratio)
{
	GstState state, pending;
	GstStateChangeReturn ret;
	pts_t pts;
	bool unpause;
	bool validposition;

	if (!m_gst_playbin)
	{
		return -1;
	}

	if (ratio > -0.01 && ratio < 0.01)
	{
		int pos_ret = -1;
		if (m_last_seek_pos > 0)
		{
			pts = m_last_seek_pos;
			pos_ret = 1;
		}
		else
		{
			pos_ret = getPlayPosition(pts);
		}
		gst_element_set_state(m_gst_playbin, GST_STATE_PAUSED);
		if (pos_ret >= 0)
		{
			seekTo(pts);
		}
		/* Pipeline sometimes blocks due to audio track issue of gstreamer.
		If the pipeline is blocked up on pending state change to paused,
		this issue is solved by seek to play position. */
		ret = gst_element_get_state(m_gst_playbin, &state, &pending, 3LL * GST_MSECOND);
		if (state == GST_STATE_PLAYING && pending == GST_STATE_PAUSED)
		{
			if (pos_ret >= 0)
			{
				eDebug("[eServiceMP3::%s] blocked pipeline; we need to flush playposition in pts at paused is %" G_GINT64_FORMAT, __func__, (gint64)pts);
				seekTo(pts);
			}
		}
		return 0;
	}

	unpause = (m_currentTrickRatio == 1.0 && ratio == 1.0);
	if (unpause)
	{
		GstElement *source = NULL;
		GstElementFactory *factory = NULL;
		const gchar *name = NULL;
		g_object_get(m_gst_playbin, "source", &source, NULL);
		if (!source)
		{
			eDebugNoNewLineStart("[eServiceMP3::%s] cannot get source", __func__);
			goto seek_unpause;
		}
		factory = gst_element_get_factory(source);
		g_object_unref(source);
		if (!factory)
		{
			eDebugNoNewLineStart("[eServiceMP3::%s] cannot get source factory", __func__);
			goto seek_unpause;
		}
		name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
		if (!name)
		{
			eDebugNoNewLineStart("[eServiceMP3::%s] cannot get source name", __func__);
			goto seek_unpause;
		}
		/*
		 * We know that filesrc and souphttpsrc will not timeout after long pause
		 * If there are other sources which will not timeout, add them here
		*/
		if (!strcmp(name, "filesrc") || !strcmp(name, "souphttpsrc"))
		{
			/* previous state was already ok if we come here just give all elements time to unpause */
			gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
			ret = gst_element_get_state(m_gst_playbin, &state, &pending, 0);
			eDebug("[eServiceMP3::%s] unpause state:%s pending:%s ret:%s", __func__,
				gst_element_state_get_name(state),
				gst_element_state_get_name(pending),
				gst_element_state_change_return_get_name(ret));
			return 0;
		}
		else
		{
			eDebugNoNewLineStart("[eServiceMP3::%s] source '%s' is not supported", __func__, name);
		}
seek_unpause:
		eDebug(", doing seeking unpause");
	}

	m_currentTrickRatio = ratio;

	validposition = false;
	gint64 pos = 0;
	if (m_last_seek_pos > 0)
	{
		validposition = true;
		pos = m_last_seek_pos * 11111LL;
	}
	else if (getPlayPosition(pts) >= 0)
	{
		validposition = true;
		pos = pts * 11111LL;
	}

	ret = gst_element_get_state(m_gst_playbin, &state, &pending, 2 * GST_SECOND);
	if (state != GST_STATE_PLAYING)
	{
		eDebug("[eServiceMP3::%s] set unpause or change playrate when gst was state: %s, pending: %s, ret: %s", __func__,
				gst_element_state_get_name(state),
				gst_element_state_get_name(pending),
				gst_element_state_change_return_get_name(ret));
		gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
	}

	if (validposition)
	{
		if (ratio >= 0.0)
		{
			gst_element_seek(m_gst_playbin, ratio, GST_FORMAT_TIME,
				(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO),
				GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_SET, -1);
		}
		else
		{
			/* note that most elements will not support negative speed */
			gst_element_seek(m_gst_playbin, ratio, GST_FORMAT_TIME,
				(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_TRICKMODE | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO),
				GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, pos);
		}
	}

	m_prev_decoder_time = -1;
	m_decoder_time_valid_state = 0;
	return 0;
}
#endif

RESULT eServiceMP3::seekRelative(int direction, pts_t to)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_gst_playbin)
	{
		return -1;
	}
#endif
	gint64 ppos = 0;
	if (direction > 0)
	{
		if (m_last_seek_pos > 0)
		{
			ppos = m_last_seek_pos + to;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
			return seekTo(ppos);
#else // eplayer3
			return player->Seek((int64_t)to * direction * AV_TIME_BASE / 90000, false);
#endif
		}
		else
		{
			if (getPlayPosition(ppos) < 0)
			{
				return -1;
			}
			ppos += to;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
			return seekTo(ppos);
#else // eplayer3
			return player->Seek((int64_t)to * direction * AV_TIME_BASE / 90000, false);
#endif
		}
	}
	else
	{
		if (m_last_seek_pos > 0)
		{
			ppos = m_last_seek_pos - to;
			if (ppos < 0)
			{
				ppos = 0;
			}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
			return seekTo(ppos);
#else // eplayer3
			return player->Seek((int64_t)to * direction * AV_TIME_BASE / 90000, false);
#endif
		}
		else
		{
			if (getPlayPosition(ppos) < 0)
			{
				return -1;
			}
			ppos -= to;
			if (ppos < 0)
			{
				ppos = 0;
			}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
			return seekTo(ppos);
#else // eplayer3
			return player->Seek((int64_t)to * direction * AV_TIME_BASE / 90000, false);
#endif
		}
	}
}

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
gint eServiceMP3::match_sinktype(const GValue *velement, const gchar *type)
{
	GstElement *element = GST_ELEMENT_CAST(g_value_get_object(velement));
	return strcmp(g_type_name(G_OBJECT_TYPE(element)), type);
}
#endif

RESULT eServiceMP3::getPlayPosition(pts_t &pts)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	gint64 pos = 0;

	if (!m_gst_playbin || m_state != stRunning)
	{
		return -1;
	}
	// Allow only one ioctl call per second.
	// In case of seek procedure, the position
	// is updated by the seektoImpl function.
	if (!m_use_last_seek)
	{
		//eDebug("[eServiceMP3] start use last seek timer");
		m_use_last_seek = true;
		m_play_position_timer->start(1000, true);
	}
	else
	{
		pts = m_last_seek_pos;
		return 0;
	}

	if ((dvb_audiosink || dvb_videosink) && !m_paused && !m_sourceinfo.is_hls)
	{
		if (m_sourceinfo.is_audio)
		{
			g_signal_emit_by_name(dvb_audiosink, "get-decoder-time", &pos);
		}
		else
		{
			/* Most STB's work better when pts is taken by audio but some video must be taken when audio is 0 or invalid */
			/* Avoid taking the audio play position if audio sink is in state NULL */
			g_signal_emit_by_name(dvb_videosink, "get-decoder-time", &pos);
			if ((!m_audiosink_not_running && !GST_CLOCK_TIME_IS_VALID(pos)) || 0)
			{
				 g_signal_emit_by_name(dvb_audiosink, "get-decoder-time", &pos);
			}
		}
		if (!GST_CLOCK_TIME_IS_VALID(pos))
		{
			return -1;
		}
	}
	else
	{
		if (m_paused && m_last_seek_pos > 0)
		{
			pts = m_last_seek_pos;
			return 0;
		}
		GstFormat fmt = GST_FORMAT_TIME;
		if (!gst_element_query_position(m_gst_playbin, fmt, &pos))
		{
			eDebug("[eServiceMP3::%s] gst_element_query_position failed", __func__);
			return -1;
		}
	}

	/* pos is in nanoseconds. There are have 90 000 pts per second. */
	m_last_seek_pos = pos / 11111LL;
	pts = m_last_seek_pos;
#else // eplayer3 code
	pts = 0;

	if (m_state != stRunning)
	{
		return -1;
	}
	if (!player->isPlaying)
	{
		eDebug("[eServiceMP3::%s] EOF!", __func__);
		m_event((iPlayableService*)this, evEOF);
		return -1;
	}

	int64_t vpts = 0;
	player->GetPts(vpts);

	if (vpts <= 0)
	{
		return -1;
	}
	/* len is in nanoseconds. There are 90.000 pts per second. */
	pts = vpts;
#endif
	return 0;
}

RESULT eServiceMP3::setTrickmode(int trick)
{
	/* trickmode is not yet supported by our dvbmediasinks. */
	return -1;
}

RESULT eServiceMP3::isCurrentlySeekable()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
// Hellmaster1024: 1 for skipping 3 for skipping and fast forward
	int ret = 3; /* just assume that seeking and fast/slow winding are possible */

	if (!m_gst_playbin)
	{
		return 0;
	}
	return ret;
#else  // eplayer3
	// Hellmaster1024: 1 for skipping 3 for skipping and fast forward
	return 3;
#endif
}

RESULT eServiceMP3::info(ePtr<iServiceInformation>&i)
{
	i = this;
	return 0;
}

RESULT eServiceMP3::getName(std::string &name)
{
	std::string title = m_ref.getName();
	if (title.empty())
	{
		name = m_ref.path;
		size_t n = name.rfind('/');
		if (n != std::string::npos)
		{
			name = name.substr(n + 1);
		}
	}
	else
	{
		name = title;
	}
	return 0;
}

RESULT eServiceMP3::getEvent(ePtr<eServiceEvent> &evt, int nownext)
{
	evt = nownext ? m_event_next : m_event_now;
	if (!evt)
	{
		return -1;
	}
	return 0;
}

int eServiceMP3::getInfo(int w)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	const gchar *tag = 0;
#endif

	switch (w)
	{
		case sServiceref:
		{
			return m_ref;
		}
		case sVideoHeight:
		{
			return m_height;
		}
		case sVideoWidth:
		{
			return m_width;
		}
		case sFrameRate:
		{
			return m_framerate;
		}
		case sProgressive:
		{
			return m_progressive;
		}
		case sGamma:
		{
			return m_gamma;
		}
		case sAspect:
		{
			return m_aspect;
		}
		case sTagTitle:
		case sTagArtist:
		case sTagAlbum:
		case sTagTitleSortname:
		case sTagArtistSortname:
		case sTagAlbumSortname:
		case sTagDate:
		case sTagComposer:
		case sTagGenre:
		case sTagComment:
		case sTagExtendedComment:
		case sTagLocation:
		case sTagHomepage:
		case sTagDescription:
		case sTagVersion:
		case sTagISRC:
		case sTagOrganization:
		case sTagCopyright:
		case sTagCopyrightURI:
		case sTagContact:
		case sTagLicense:
		case sTagLicenseURI:
		case sTagCodec:
		case sTagAudioCodec:
		case sTagVideoCodec:
		case sTagEncoder:
		case sTagLanguageCode:
		case sTagKeywords:
		case sTagChannelMode:
		case sUser + 12:
#if not defined(__sh__)
		{
			return resIsString;
		}
#endif
		case sTagTrackGain:
		case sTagTrackPeak:
		case sTagAlbumGain:
		case sTagAlbumPeak:
		case sTagReferenceLevel:
		case sTagBeatsPerMinute:
		case sTagImage:
		case sTagPreviewImage:
		case sTagAttachment:
		{
			return resIsPyObject;
		}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
		case sTagTrackNumber:
		{
			tag = GST_TAG_TRACK_NUMBER;
			break;
		}
		case sTagTrackCount:
		{
			tag = GST_TAG_TRACK_COUNT;
			break;
		}
		case sTagAlbumVolumeNumber:
		{
			tag = GST_TAG_ALBUM_VOLUME_NUMBER;
			break;
		}
		case sTagAlbumVolumeCount:
		{
			tag = GST_TAG_ALBUM_VOLUME_COUNT;
			break;
		}
		case sTagBitrate:
		{
			tag = GST_TAG_BITRATE;
			break;
		}
		case sTagNominalBitrate:
		{
			tag = GST_TAG_NOMINAL_BITRATE;
			break;
		}
		case sTagMinimumBitrate:
		{
			tag = GST_TAG_MINIMUM_BITRATE;
			break;
		}
		case sTagMaximumBitrate:
		{
			tag = GST_TAG_MAXIMUM_BITRATE;
			break;
		}
		case sTagSerial:
		{
			tag = GST_TAG_SERIAL;
			break;
		}
		case sTagEncoderVersion:
		{
			tag = GST_TAG_ENCODER_VERSION;
			break;
		}
		case sTagCRC:
		{
			tag = "has-crc";
			break;
		}
#endif
		case sBuffer:
		{
			return m_bufferInfo.bufferPercent;
		}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
		case sVideoType:
		{
			if (!dvb_videosink)
			{
				return -1;
			}
			guint64 v = -1;
			g_signal_emit_by_name(dvb_videosink, "get-video-codec", &v);
			return (int) v;
		}
#endif
		case sSID:
		{
			return m_ref.getData(1);
		}
		default:
		{
			return resNA;
		}
	}

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_stream_tags || !tag)
	{
		return 0;
	}
	guint value;
	if (gst_tag_list_get_uint(m_stream_tags, tag, &value))
	{
		return (int)value;
	}
#endif
	return 0;
}

std::string eServiceMP3::getInfoString(int w)
{
	switch (w)
	{
		case sProvider:
		{
			return m_sourceinfo.is_streaming ? "IPTV" : "FILE";
		}
		case sServiceref:
		{
			return m_ref.toString();
		}
		default:
		{
			break;
		}
	}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (!m_stream_tags && w < sUser && w > 26)
	{
		return "";
	}
	const gchar *tag = 0;
	switch (w)
	{
		case sTagTitle:
		{
			tag = GST_TAG_TITLE;
			break;
		}
		case sTagArtist:
		{
			tag = GST_TAG_ARTIST;
			break;
		}
		case sTagAlbum:
		{
			tag = GST_TAG_ALBUM;
			break;
		}
		case sTagTitleSortname:
		{
			tag = GST_TAG_TITLE_SORTNAME;
			break;
		}
		case sTagArtistSortname:
		{
			tag = GST_TAG_ARTIST_SORTNAME;
			break;
		}
		case sTagAlbumSortname:
		{
			tag = GST_TAG_ALBUM_SORTNAME;
			break;
		}
		case sTagDate:
		{
			GDate *date;
			GstDateTime *date_time;
			if (gst_tag_list_get_date(m_stream_tags, GST_TAG_DATE, &date))
			{
				gchar res[8];
				snprintf(res, sizeof(res), "%04d", g_date_get_year(date));
				g_date_free(date);
				return (std::string)res;
			}
			else if (gst_tag_list_get_date_time(m_stream_tags, GST_TAG_DATE_TIME, &date_time))
			{
				if (gst_date_time_has_year(date_time))
				{
					gchar res[8];
					snprintf(res, sizeof(res), "%04d", gst_date_time_get_year(date_time));
					gst_date_time_unref(date_time);
					return (std::string)res;
				}
				gst_date_time_unref(date_time);
			}
			break;
		}
		case sTagComposer:
		{
			tag = GST_TAG_COMPOSER;
			break;
		}
		case sTagGenre:
		{
			tag = GST_TAG_GENRE;
			break;
		}
		case sTagComment:
		{
			tag = GST_TAG_COMMENT;
			break;
		}
		case sTagExtendedComment:
		{
			tag = GST_TAG_EXTENDED_COMMENT;
			break;
		}
		case sTagLocation:
		{
			tag = GST_TAG_LOCATION;
			break;
		}
		case sTagHomepage:
		{
			tag = GST_TAG_HOMEPAGE;
			break;
		}
		case sTagDescription:
		{
			tag = GST_TAG_DESCRIPTION;
			break;
		}
		case sTagVersion:
		{
			tag = GST_TAG_VERSION;
			break;
		}
		case sTagISRC:
		{
			tag = GST_TAG_ISRC;
			break;
		}
		case sTagOrganization:
		{
			tag = GST_TAG_ORGANIZATION;
			break;
		}
		case sTagCopyright:
		{
			tag = GST_TAG_COPYRIGHT;
			break;
		}
		case sTagCopyrightURI:
		{
			tag = GST_TAG_COPYRIGHT_URI;
			break;
		}
		case sTagContact:
		{
			tag = GST_TAG_CONTACT;
			break;
		}
		case sTagLicense:
		{
			tag = GST_TAG_LICENSE;
			break;
		}
		case sTagLicenseURI:
		{
			tag = GST_TAG_LICENSE_URI;
			break;
		}
		case sTagCodec:
		{
			tag = GST_TAG_CODEC;
			break;
		}
		case sTagAudioCodec:
		{
			tag = GST_TAG_AUDIO_CODEC;
			break;
		}
		case sTagVideoCodec:
		{
			tag = GST_TAG_VIDEO_CODEC;
			break;
		}
		case sTagEncoder:
		{
			tag = GST_TAG_ENCODER;
			break;
		}
		case sTagLanguageCode:
		{
			tag = GST_TAG_LANGUAGE_CODE;
			break;
		}
		case sTagKeywords:
		{
			tag = GST_TAG_KEYWORDS;
			break;
		}
		case sTagChannelMode:
		{
			tag = "channel-mode";
			break;
		}
		case sUser+12:
		{
			return m_errorInfo.error_message;
		}
		default:
		{
			return "";
		}
	}
	if (!tag)
	{
		return "";
	}
	if ( !tag )
	{
		return "";
	}
	gchar *value = NULL;
	if (m_stream_tags && gst_tag_list_get_string(m_stream_tags, tag, &value))
	{
		std::string res = value;
		g_free(value);
		return res;
	}
#else  // eplayer3
	switch (w)
	{
		case sTagTitle:
		case sTagTitleSortname:
		{
			return getTag("title");
		}
		case sTagArtist:
		case sTagArtistSortname:
		{
			return getTag("artist");
		}
		case sTagAlbum:
		{
			return getTag("album");
		}
		case sTagComment:
		case sTagExtendedComment:
		{
			return getTag("comment");
		}
		case sTagGenre:
		{
			return getTag("genre");
		}
		case sTagDate:
		{
			return getTag("date");
		}
		case sTagComposer:
		{
			return getTag("composer");
		}
		case sTagCopyright:
		{
			return getTag("copyright");
		}
		case sTagEncoder:
		{
			return getTag("encoder");
		}
		case sTagLanguageCode:
		{
			return getTag("language");
		}
		default:
		{
			break;
		}
	}
	if (player && player->playback)
	{
		/* Hellmaster1024: we need to save the address of tag to free the strduped mem
		   the command will return a new address for a new strduped string.
		   Both Strings need to be freed! */
		res_str = tag;
		player->playback->Command(player, PLAYBACK_INFO, &res_str);
		/* Hellmaster1024: in case something went wrong maybe no new adress is returned */
		if (tag != res_str)
		{
			std::string res = res_str;
			free(tag);
			free(res_str);
			return res;
		}
		else
		{
			free(tag);
			return "";
		}
	}
	free(tag);
#endif
	return "";
}

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
ePtr<iServiceInfoContainer> eServiceMP3::getInfoObject(int w)
{
	eServiceMP3InfoContainer *container = new eServiceMP3InfoContainer;
	ePtr<iServiceInfoContainer> retval = container;
	const gchar *tag = 0;
	bool isBuffer = false;
	switch (w)
	{
		case sTagTrackGain:
		{
			tag = GST_TAG_TRACK_GAIN;
			break;
		}
		case sTagTrackPeak:
		{
			tag = GST_TAG_TRACK_PEAK;
			break;
		}
		case sTagAlbumGain:
		{
			tag = GST_TAG_ALBUM_GAIN;
			break;
		}
		case sTagAlbumPeak:
		{
			tag = GST_TAG_ALBUM_PEAK;
			break;
		}
		case sTagReferenceLevel:
		{
			tag = GST_TAG_REFERENCE_LEVEL;
			break;
		}
		case sTagBeatsPerMinute:
		{
			tag = GST_TAG_BEATS_PER_MINUTE;
			break;
		}
		case sTagImage:
		{
			tag = GST_TAG_IMAGE;
			isBuffer = true;
			break;
		}
		case sTagPreviewImage:
		{
			tag = GST_TAG_PREVIEW_IMAGE;
			isBuffer = true;
			break;
		}
		case sTagAttachment:
		{
			tag = GST_TAG_ATTACHMENT;
			isBuffer = true;
			break;
		}
		default:
		{
			break;
		}
	}

	if (m_stream_tags && tag)
	{
		if (isBuffer)
		{
			const GValue *gv_buffer = gst_tag_list_get_value_index(m_stream_tags, tag, 0);
			if (gv_buffer)
			{
				GstBuffer *buffer;
				buffer = gst_value_get_buffer(gv_buffer);
				container->setBuffer(buffer);
			}
		}
		else
		{
			gdouble value = 0.0;
			gst_tag_list_get_double(m_stream_tags, tag, &value);
			container->setDouble(value);
		}
	}
	return retval;
}
#endif

RESULT eServiceMP3::audioChannel(ePtr<iAudioChannelSelection> &ptr)
{
	ptr = this;
	return 0;
}

RESULT eServiceMP3::audioTracks(ePtr<iAudioTrackSelection> &ptr)
{
	ptr = this;
	return 0;
}

RESULT eServiceMP3::cueSheet(ePtr<iCueSheet> &ptr)
{
	ptr = this;
	return 0;
}

RESULT eServiceMP3::subtitle(ePtr<iSubtitleOutput> &ptr)
{
	ptr = this;
	return 0;
}

RESULT eServiceMP3::audioDelay(ePtr<iAudioDelay> &ptr)
{
	ptr = this;
	return 0;
}

int eServiceMP3::getNumberOfTracks()
{
 	return m_audioStreams.size();
}

int eServiceMP3::getCurrentTrack()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	if (m_currentAudioStream == -1)
	{
		g_object_get(m_gst_playbin, "current-audio", &m_currentAudioStream, NULL);
	}
#endif
	return m_currentAudioStream;
}

RESULT eServiceMP3::selectTrack(unsigned int i)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	seekRelative(-1, 90000);  // flush
#endif
	return selectAudioStream(i);
}

int eServiceMP3::selectAudioStream(int i)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	int current_audio;
	g_object_set(m_gst_playbin, "current-audio", i, NULL);
	g_object_get(m_gst_playbin, "current-audio", &current_audio, NULL);
	if (current_audio == i)
	{
		eDebug("[eServiceMP3] switched to audio stream %d", current_audio);
		m_currentAudioStream = i;
		return 0;
	}
	return -1;
#else // eplayer3
	if (m_state == stRunning && i != m_currentAudioStream)
	{
		player->SwitchAudio(m_audioStreams[i].pid);
		seekRelative(-1, 5000);
		eDebug("[eServiceMP3] switched to audio stream %d", current_audio);
		m_currentAudioStream = i;
		m_metaData.clear();
		return 0;
	}
	return -1;
#endif
}

int eServiceMP3::getCurrentChannel()
{
	return STEREO;
}

RESULT eServiceMP3::selectChannel(int i)
{
	eDebug("[eServiceMP3] selectChannel(%i)",i);
	return 0;
}

RESULT eServiceMP3::getTrackInfo(struct iAudioTrackInfo &info, unsigned int i)
{
	if (i >= m_audioStreams.size())
	{
		return -2;
	}
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	info.m_description = m_audioStreams[i].codec;
	if (info.m_language.empty())
	{
		info.m_language = m_audioStreams[i].language_code;
	}
	return 0;
#else // eplayer3
	switch(m_audioStreams[i].type)
	{
		case 1:
		{
			info.m_description = "MPEG";
			break;
		}
		case 2:
		{
			info.m_description = "MP3";
			break;
		}
		case 3:
		{
			info.m_description = "AC3";
			break;
		}
		case 4:
		{
			info.m_description = "DTS";
			break;
		}
		case 5:
		{
			info.m_description = "AAC";
			break;
		}
		case 0:
		case 6:
		{
			info.m_description = "PCM";
			break;
		}
		case 8:
		{
			info.m_description = "FLAC";
			break;
		}
		case 9:
		{
			info.m_description = "WMA";
			break;
		}
		default:
		{
			break;
		}
	}
	if (info.m_language.empty())
	{
		info.m_language = m_audioStreams[i].language_code;
	}
	return 0;
#endif
}


#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
subtype_t getSubtitleType(GstPad* pad, gchar *g_codec=NULL)
{
	subtype_t type = stUnknown;
	GstCaps* caps = gst_pad_get_current_caps(pad);
	if (!caps && !g_codec)
	{
		caps = gst_pad_get_allowed_caps(pad);
	}

	if (caps && !gst_caps_is_empty(caps))
	{
		GstStructure* str = gst_caps_get_structure(caps, 0);
		if (str)
		{
			const gchar *g_type = gst_structure_get_name(str);
			eDebug("[eServiceMP3::%s] subtitle probe caps type=%s", __func__, g_type ? g_type : "(null)");
			if (g_type)
			{
				if (!strcmp(g_type, "subpicture/x-dvd"))
				{
					type = stVOB;
				}
				else if (!strcmp(g_type, "text/x-pango-markup"))
				{
					type = stSRT;
				}
				else if (!strcmp(g_type, "text/plain")
				||       !strcmp(g_type, "text/x-plain")
				||       !strcmp(g_type, "text/x-raw"))
				{
					type = stPlainText;
				}
				else if (!strcmp(g_type, "subpicture/x-pgs"))
				{
					type = stPGS;
				}
				else
				{
					eDebug("[eServiceMP3::%s] unsupported subtitle caps %s (%s)", __func__, g_type, g_codec ? g_codec : "(null)");
				}
			}
		}
	}
	else if (g_codec)
	{
		eDebug("[eServiceMP3::%s] subtitle probe codec tag=%s", __func__, g_codec);
		if (!strcmp(g_codec, "VOB"))
		{
			type = stVOB;
		}
		else if (!strcmp(g_codec, "SubStation Alpha")
		||       !strcmp(g_codec, "SSA"))
		{
			type = stSSA;
		}
		else if (!strcmp(g_codec, "ASS"))
		{
			type = stASS;
		}
		else if (!strcmp(g_codec, "SRT"))
		{
			type = stSRT;
		}
		else if (!strcmp(g_codec, "UTF-8 plain text"))
		{
			type = stPlainText;
		}
		else
		{
			eDebug("[eServiceMP3::%s] unsupported subtitle codec %s", __func__, g_codec);
		}
	}
	else
	{
		eDebug("[eServiceMP3::%s] unidentifiable subtitle stream", __func__);
	}
	return type;
}

void eServiceMP3::gstBusCall(GstMessage *msg)
{
	if (!msg)
	{
		return;
	}
	gchar *sourceName;
	GstObject *source;
	source = GST_MESSAGE_SRC(msg);
	if (!GST_IS_OBJECT(source))
	{
		return;
	}
	sourceName = gst_object_get_name(source);
	GstState state, pending;
	switch (GST_MESSAGE_TYPE(msg))
	{
		case GST_MESSAGE_EOS:
		{
			eDebug("[eServiceMP3::%s] EOS received", __func__);
			m_event((iPlayableService*)this, evEOF);
			break;
		}
		case GST_MESSAGE_STATE_CHANGED:
		{
			if (GST_MESSAGE_SRC(msg) != GST_OBJECT(m_gst_playbin))
			{
				break;
			}
			GstState old_state, new_state;
			GstStateChange transition;
			gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);

			if (old_state == new_state)
			{
				break;
			}
			std::string s_old_state(gst_element_state_get_name(old_state));
			std::string s_new_state(gst_element_state_get_name(new_state));
			eDebug("[eServiceMP3] state transition %s -> %s", s_old_state.c_str(), s_new_state.c_str());

			if (m_gstdot)
			{
				std::string s_graph_filename = "GStreamer-enigma2." + s_old_state + "_" + s_new_state;
				GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_gst_playbin), GST_DEBUG_GRAPH_SHOW_ALL, s_graph_filename.c_str());
			}
			transition = (GstStateChange)GST_STATE_TRANSITION(old_state, new_state);

			switch (transition)
			{
				GstStateChangeReturn ret;
				case GST_STATE_CHANGE_NULL_TO_READY:
				{
					m_first_paused = true;
					m_event(this, evStart);
					if (!m_is_live)
					{
						gst_element_set_state(m_gst_playbin, GST_STATE_PAUSED);
					}
					ret = gst_element_get_state(m_gst_playbin, &state, &pending, 5LL * GST_SECOND);
					eDebug("[eServiceMP3::%s] PLAYBIN WITH BLOCK READY TO PAUSED state:%s pending:%s ret:%s", __func__,
						gst_element_state_get_name(state),
						gst_element_state_get_name(pending),
						gst_element_state_change_return_get_name(ret));
					if (ret == GST_STATE_CHANGE_NO_PREROLL)
					{
						gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
						m_is_live = true;
					}
					break;
				}
				case GST_STATE_CHANGE_READY_TO_PAUSED:
				{
					m_state = stRunning;
					m_event(this, evStart);
					setAC3Delay(ac3_delay);
					setPCMDelay(pcm_delay);
					if (!m_sourceinfo.is_streaming && !m_cuesheet_loaded) /* cuesheet CVR */
					{
						loadCuesheet();
					}
					updateEpgCacheNowNext();
					/* avoid position taking on audiosink when audiosink is not running */
					ret = gst_element_get_state(dvb_audiosink, &state, &pending, 3 * GST_SECOND);
					if (state == GST_STATE_NULL)
					{
						m_audiosink_not_running = true;
					}
					if (!m_is_live)
					{
						gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
					}
					/* tempo debug */
					/* wait on async state change complete for max 5 seconds */
					ret = gst_element_get_state(m_gst_playbin, &state, &pending, 3 * GST_SECOND);
					eDebug("[eServiceMP3::%s] PLAYBIN WITH BLOCK PLAYSTART state:%s pending:%s ret:%s", __func__,
						gst_element_state_get_name(state),
						gst_element_state_get_name(pending),
						gst_element_state_change_return_get_name(ret));
					if (!m_is_live && ret == GST_STATE_CHANGE_NO_PREROLL)
					{
						m_is_live = true;
					}
					m_event((iPlayableService*)this, evGstreamerPlayStarted);

					if (!dvb_videosink || m_ref.getData(0) == 2) // show radio pic
					{
						bool showRadioBackground = eConfigManager::getConfigBoolValue("config.misc.showradiopic", true);
						std::string radio_pic = eConfigManager::getConfigValue(showRadioBackground ? "config.misc.radiopic" : "config.misc.blackradiopic");
						m_decoder = new eTSMPEGDecoder(NULL, 0);
						m_decoder->showSinglePic(radio_pic.c_str());
					}
					break;
				}
				case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
				{
					if (m_sourceinfo.is_streaming && m_streamingsrc_timeout)
					{
						m_streamingsrc_timeout->stop();
					}
					m_paused = false;
					if (m_autoaudio)
					{
						unsigned int autoaudio = 0;
						int autoaudio_level = 5;
						std::string configvalue;
						std::vector<std::string> autoaudio_languages;
						configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect1");
						if (configvalue != "" && configvalue != "None")
						{
							autoaudio_languages.push_back(configvalue);
						}
						configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect2");
						if (configvalue != "" && configvalue != "None")
						{
							autoaudio_languages.push_back(configvalue);
						}
						configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect3");
						if (configvalue != "" && configvalue != "None")
						{
							autoaudio_languages.push_back(configvalue);
						}
						configvalue = eConfigManager::getConfigValue("config.autolanguage.audio_autoselect4");
						if (configvalue != "" && configvalue != "None")
						{
							autoaudio_languages.push_back(configvalue);
						}
						for (unsigned int i = 0; i < m_audioStreams.size(); i++)
						{
							if (!m_audioStreams[i].language_code.empty())
							{
								int x = 1;
								for (std::vector<std::string>::iterator it = autoaudio_languages.begin(); x < autoaudio_level && it != autoaudio_languages.end(); x++, it++)
								{
									if ((*it).find(m_audioStreams[i].language_code) != std::string::npos)
									{
										autoaudio = i;
										autoaudio_level = x;
										break;
									}
								}
							}
						}

						if (autoaudio)
						{
							selectTrack(autoaudio);
						}
						m_autoaudio = false;
					}
					if (!m_first_paused)
					{
						m_event((iPlayableService*)this, evGstreamerPlayStarted);
					}
					else
					{
						m_first_paused = false;
					}
					break;
				}
				case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
				{
					m_paused = true;
					break;
				}
				case GST_STATE_CHANGE_PAUSED_TO_READY:
				case GST_STATE_CHANGE_READY_TO_NULL:
				case GST_STATE_CHANGE_NULL_TO_NULL:
				case GST_STATE_CHANGE_READY_TO_READY:
				case GST_STATE_CHANGE_PAUSED_TO_PAUSED:
				case GST_STATE_CHANGE_PLAYING_TO_PLAYING:
				{
					break;
				}
			}
			break;
		}
		case GST_MESSAGE_ERROR:
		{
			gchar *debug;
			GError *err;
			gst_message_parse_error(msg, &err, &debug);
			g_free(debug);
			eWarning("[eServiceMP3::%s] Gstreamer error: %s (%i) from %s", __func__, err->message, err->code, sourceName );
			if (err->domain == GST_STREAM_ERROR)
			{
				if (err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND)
				{
					if (g_strrstr(sourceName, "videosink"))
					{
						m_event((iPlayableService*)this, evUser+11);
					}
					else if (g_strrstr(sourceName, "audiosink"))
					{
						m_event((iPlayableService*)this, evUser+10);
					}
				}
			}
			else if (err->domain == GST_RESOURCE_ERROR)
			{
				if (err->code == GST_RESOURCE_ERROR_OPEN_READ
				||  err->code == GST_RESOURCE_ERROR_READ)
				{
					stop();
				}
			}
			g_error_free(err);
			break;
		}
		case GST_MESSAGE_WARNING:
		{
			gchar *debug_warn = NULL;
			GError *warn = NULL;
			gst_message_parse_warning(msg, &warn, &debug_warn);
			/* CVR this Warning occurs from time to time with external srt files
			When a new seek is done the problem off to long wait times before subtitles appears,
			after movie was restarted with a resume position is solved. */
			if (!strncmp(warn->message , "Internal data flow problem", 26) && !strncmp(sourceName, "subtitle_sink", 13))
			{
				eWarning("[eServiceMP3::%s] Gstreamer warning : %s (%i) from %s", __func__, warn->message, warn->code, sourceName);
				if (dvb_subsink)
				{
					if (!gst_element_seek(dvb_subsink, m_currentTrickRatio, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
						GST_SEEK_TYPE_SET, (gint64)(m_last_seek_pos * 11111LL),
						GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
					{
						eDebug("[eServiceMP3::%s] seekToImpl subsink failed", __func__);
					}
				}
			}
			g_free(debug_warn);
			g_error_free(warn);
			break;
		}
		case GST_MESSAGE_INFO:
		{
			gchar *debug;
			GError *inf;

			gst_message_parse_info(msg, &inf, &debug);
			g_free(debug);
			if (inf->domain == GST_STREAM_ERROR && inf->code == GST_STREAM_ERROR_DECODE)
			{
				if (g_strrstr(sourceName, "videosink"))
				{
					m_event((iPlayableService*)this, evUser+14);
				}
			}
			g_error_free(inf);
			break;
		}
		case GST_MESSAGE_TAG:
		{
			GstTagList *tags, *result;
			gst_message_parse_tag(msg, &tags);

			result = gst_tag_list_merge(m_stream_tags, tags, GST_TAG_MERGE_REPLACE);
			if (result)
			{
				if (m_stream_tags && gst_tag_list_is_equal(m_stream_tags, result))
				{
					gst_tag_list_free(tags);
					gst_tag_list_free(result);
					break;
				}
				if (m_stream_tags)
				{
					gst_tag_list_free(m_stream_tags);
				}
				m_stream_tags = result;
			}

			if (!m_coverart)
			{
				const GValue *gv_image = gst_tag_list_get_value_index(tags, GST_TAG_IMAGE, 0);
				if (gv_image)
				{
					GstBuffer *buf_image;
					GstSample *sample;
					sample = (GstSample *)g_value_get_boxed(gv_image);
					buf_image = gst_sample_get_buffer(sample);
					int fd = open("/tmp/.id3coverart", O_CREAT|O_WRONLY|O_TRUNC, 0644);
					if (fd >= 0)
					{
						guint8 *data;
						gsize size;
						GstMapInfo map;
						gst_buffer_map(buf_image, &map, GST_MAP_READ);
						data = map.data;
						size = map.size;
						int ret = write(fd, data, size);
						gst_buffer_unmap(buf_image, &map);
						close(fd);
						m_coverart = true;
						m_event((iPlayableService*)this, evUser+13);
						eDebug("[eServiceMP3::%s] /tmp/.id3coverart %d bytes written ", __func__, ret);
					}
				}
			}
			gst_tag_list_free(tags);
			m_event((iPlayableService*)this, evUpdatedInfo);
			break;
		}
		/* TOC entry intercept used for chapter support CVR */
		case GST_MESSAGE_TOC:
		{
			if (!m_sourceinfo.is_audio && !m_sourceinfo.is_streaming)
			{
				HandleTocEntry(msg);
			}
			break;
		}
		case GST_MESSAGE_ASYNC_DONE:
		{
			if (GST_MESSAGE_SRC(msg) != GST_OBJECT(m_gst_playbin))
			{
				break;
			}
			gint i, n_video = 0, n_audio = 0, n_text = 0;

			g_object_get(m_gst_playbin, "n-video", &n_video, NULL);
			g_object_get(m_gst_playbin, "n-audio", &n_audio, NULL);
			g_object_get(m_gst_playbin, "n-text", &n_text, NULL);

			eDebug("[eServiceMP3::%s] async-done - %d video, %d audio, %d subtitle", __func__, n_video, n_audio, n_text);

			if (n_video + n_audio <= 0)
			{
				stop();
			}
			m_audioStreams.clear();
			m_subtitleStreams.clear();

			for (i = 0; i < n_audio; i++)
			{
				audioStream audio;
				gchar *g_codec, *g_lang;
				GstTagList *tags = NULL;
				GstPad* pad = 0;
				g_signal_emit_by_name(m_gst_playbin, "get-audio-pad", i, &pad);
				GstCaps* caps = gst_pad_get_current_caps(pad);
				gst_object_unref(pad);
				if (!caps)
				{
					continue;
				}
				GstStructure* str = gst_caps_get_structure(caps, 0);
				const gchar *g_type = gst_structure_get_name(str);
				eDebug("[eServiceMP3::%s] AUDIO STRUCT=%s", __func__, g_type);
				audio.type = gstCheckAudioPad(str);
				audio.language_code = "und";
				audio.codec = g_type;
				g_codec = NULL;
				g_lang = NULL;
				g_signal_emit_by_name(m_gst_playbin, "get-audio-tags", i, &tags);
				if (tags && GST_IS_TAG_LIST(tags))
				{
					if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &g_codec))
					{
						audio.codec = std::string(g_codec);
						g_free(g_codec);
					}
					if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &g_lang))
					{
						audio.language_code = std::string(g_lang);
						g_free(g_lang);
					}
					gst_tag_list_free(tags);
				}
				eDebug("[eServiceMP3::%s] audio stream=%i codec=%s language=%s", __func__, i, audio.codec.c_str(), audio.language_code.c_str());
				m_audioStreams.push_back(audio);
				gst_caps_unref(caps);
			}

			for (i = 0; i < n_text; i++)
			{
				gchar *g_codec = NULL, *g_lang = NULL;
				GstTagList *tags = NULL;
				g_signal_emit_by_name(m_gst_playbin, "get-text-tags", i, &tags);
				subtitleStream subs;
				subs.language_code = "und";
				if (tags && GST_IS_TAG_LIST(tags))
				{
					if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &g_lang))
					{
						subs.language_code = g_lang;
						g_free(g_lang);
					}
					gst_tag_list_get_string(tags, GST_TAG_SUBTITLE_CODEC, &g_codec);
					gst_tag_list_free(tags);
				}

				eDebug("[eServiceMP3::%s] subtitle stream=%i language=%s codec=%s", __func__, i, subs.language_code.c_str(), g_codec ? g_codec : "(null)");

				GstPad* pad = 0;
				g_signal_emit_by_name(m_gst_playbin, "get-text-pad", i, &pad);
				if (pad)
				{
					g_signal_connect(G_OBJECT (pad), "notify::caps", G_CALLBACK (gstTextpadHasCAPS), this);
				}
				subs.type = getSubtitleType(pad, g_codec);
				gst_object_unref(pad);
				g_free(g_codec);
				m_subtitleStreams.push_back(subs);
			}

			if (m_errorInfo.missing_codec != "")
			{
				if (m_errorInfo.missing_codec.find("video/") == 0 || (m_errorInfo.missing_codec.find("audio/") == 0 && m_audioStreams.empty()))
				{
					m_event((iPlayableService*)this, evUser+12);
				}
			}
			break;
		}
		case GST_MESSAGE_ELEMENT:
		{
			const GstStructure *msgstruct = gst_message_get_structure(msg);
			if (msgstruct)
			{
				if (gst_is_missing_plugin_message(msg))
				{
					GstCaps *caps = NULL;
					gst_structure_get(msgstruct, "detail", GST_TYPE_CAPS, &caps, NULL);
					if (caps)
					{
						std::string codec = (const char*) gst_caps_to_string(caps);
						gchar *description = gst_missing_plugin_message_get_description(msg);
						if (description)
						{
							eDebug("[eServiceMP3::%s] m_errorInfo.missing_codec = %s", __func__, codec.c_str());
							m_errorInfo.error_message = "GStreamer plugin " + (std::string)description + " not available!\n";
							m_errorInfo.missing_codec = codec.substr(0,(codec.find_first_of(',')));
							g_free(description);
						}
						gst_caps_unref(caps);
					}
				}
				else
				{
					const gchar *eventname = gst_structure_get_name(msgstruct);
					if (eventname)
					{
						if (!strcmp(eventname, "eventSizeChanged")
						||  !strcmp(eventname, "eventSizeAvail"))
						{
							gst_structure_get_int(msgstruct, "aspect_ratio", &m_aspect);
							gst_structure_get_int(msgstruct, "width", &m_width);
							gst_structure_get_int(msgstruct, "height", &m_height);
							if (strstr(eventname, "Changed"))
							{
								m_event((iPlayableService*)this, evVideoSizeChanged);
							}
						}
						else if (!strcmp(eventname, "eventFrameRateChanged")
						||       !strcmp(eventname, "eventFrameRateAvail"))
						{
							gst_structure_get_int(msgstruct, "frame_rate", &m_framerate);
							if (strstr(eventname, "Changed"))
							{
								m_event((iPlayableService*)this, evVideoFramerateChanged);
							}
						}
						else if (!strcmp(eventname, "eventProgressiveChanged")
						||       !strcmp(eventname, "eventProgressiveAvail"))
						{
							gst_structure_get_int(msgstruct, "progressive", &m_progressive);
							if (strstr(eventname, "Changed"))
							{
								m_event((iPlayableService*)this, evVideoProgressiveChanged);
							}
						}
 						else if (!strcmp(eventname, "eventGammaChanged"))
 						{
 							gst_structure_get_int(msgstruct, "gamma", &m_gamma);
 							if (strstr(eventname, "Changed"))
 								m_event((iPlayableService*)this, evVideoGammaChanged);
 						}
						else if (!strcmp(eventname, "redirect"))
						{
							const char *uri = gst_structure_get_string(msgstruct, "new-location");
							eDebug("[eServiceMP3::%s] redirect to %s", __func__, uri);
							gst_element_set_state(m_gst_playbin, GST_STATE_NULL);
							g_object_set(m_gst_playbin, "uri", uri, NULL);
							gst_element_set_state(m_gst_playbin, GST_STATE_PLAYING);
						}
					}
				}
			}
			break;
		}
		case GST_MESSAGE_BUFFERING:
		{
			if (m_sourceinfo.is_streaming)
			{
				GstBufferingMode mode;
				gst_message_parse_buffering(msg, &(m_bufferInfo.bufferPercent));
				eLog(6, "[eServiceMP3::%s] Buffering %u percent done", __func__, m_bufferInfo.bufferPercent);
				gst_message_parse_buffering_stats(msg, &mode, &(m_bufferInfo.avgInRate), &(m_bufferInfo.avgOutRate), &(m_bufferInfo.bufferingLeft));
				m_event((iPlayableService*)this, evBuffering);
				/*
				 * We do not react to buffer level messages, unless we are configured to use a prefill buffer
				 * (even if we are not configured to, we still use the buffer, but we rely on it to remain at the
				 * healthy level at all times, without ever having to pause the stream)
				 *
				 * Also, it does not make sense to pause the stream if it is a live stream
				 * (in which case the sink will not produce data while paused, so we won't
				 * recover from an empty buffer)
				 */
				if (m_use_prefillbuffer && !m_is_live && !m_sourceinfo.is_hls && --m_ignore_buffering_messages <= 0)
				{
					if (m_bufferInfo.bufferPercent == 100)
					{
						/* avoid setting to play while still in async state change mode */
						gst_element_get_state(m_gst_playbin, &state, &pending, 5 * GST_SECOND);
						if (state != GST_STATE_PLAYING && !m_first_paused)
						{
							eDebug("[eServiceMP3::%s] start playing, pending state was %s", __func__, pending == GST_STATE_VOID_PENDING ? "NO_PENDING" : "A_PENDING_STATE");
							gst_element_set_state (m_gst_playbin, GST_STATE_PLAYING);
						}
						/*
						 * When we start the pipeline, the contents of the buffer will immediately drain
						 * into the (hardware buffers of the) sinks, so we will receive low buffer level
						 * messages right away.
						 * Ignore the first few buffering messages, giving the buffer the chance to recover
						 * a bit, before we start handling empty buffer states again.
						 */
						m_ignore_buffering_messages = 10;
					}
					else if (m_bufferInfo.bufferPercent == 0 && !m_first_paused)
					{
						eDebug("[eServiceMP3::%s] pause", __func__);
						gst_element_set_state(m_gst_playbin, GST_STATE_PAUSED);
						m_ignore_buffering_messages = 0;
					}
					else
					{
						m_ignore_buffering_messages = 0;
					}
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
	g_free(sourceName);
}

void eServiceMP3::handleMessage(GstMessage *msg)
{
	if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_STATE_CHANGED && GST_MESSAGE_SRC(msg) != GST_OBJECT(m_gst_playbin))
	{
		/*
		 * Ignore verbose state change messages for all active elements;
		 * we only need to handle state-change events for the playbin
		 */
		gst_message_unref(msg);
		return;
	}
	m_pump.send(new GstMessageContainer(1, msg, NULL, NULL));
}

GstBusSyncReply eServiceMP3::gstBusSyncHandler(GstBus *bus, GstMessage *message, gpointer user_data)
{
	eServiceMP3 *_this = (eServiceMP3*)user_data;
	if (_this)
	{
		_this->handleMessage(message);
	}
	return GST_BUS_DROP;
}

	/*Processing TOC CVR */
void eServiceMP3::HandleTocEntry(GstMessage *msg)
{
	/* limit TOC to dvbvideosink cue sheet only works for video media */
	if (!strncmp(GST_MESSAGE_SRC_NAME(msg), "dvbvideosink", 12))
	{
		GstToc *toc;
		gboolean updated;
		gst_message_parse_toc(msg, &toc, &updated);
		for (GList* i = gst_toc_get_entries(toc); i; i = i->next)
		{
			GstTocEntry *entry = static_cast<GstTocEntry*>(i->data);
			if (gst_toc_entry_get_entry_type(entry) == GST_TOC_ENTRY_TYPE_EDITION)
			{
				/* extra debug info for testing purposes CVR should_be_removed later on */
				eLog(5, "[eServiceMP3::%s] toc_type %s", __func__, gst_toc_entry_type_get_nick(gst_toc_entry_get_entry_type (entry)));
				gint y = 0;
				for (GList* x = gst_toc_entry_get_sub_entries(entry); x; x = x->next)
				{
					GstTocEntry *sub_entry = static_cast<GstTocEntry*>(x->data);
					if (gst_toc_entry_get_entry_type(sub_entry) == GST_TOC_ENTRY_TYPE_CHAPTER)
					{
						if (y == 0)
						{
							m_use_chapter_entries = true;
							if (m_cuesheet_loaded)
							{
								m_cue_entries.clear();
							}
							else
							{
								loadCuesheet();
							}
						}
						/* first chapter is movie start no cut needed */
						else if (y >= 1)
						{
							gint64 start = 0;
							gint64 pts = 0;
							gint type = 0;
							gst_toc_entry_get_start_stop_times(sub_entry, &start, NULL);
							type = 2;
							if (start > 0)
							{
								pts = start / 11111;
							}
							if (pts > 0)
							{
								m_cue_entries.insert(cueEntry(pts, type));
								/* extra debug info for testing purposes CVR should_be_removed later on */
								eLog(5, "[eServiceMP3::%s] toc_subtype %s,Nr = %d, start= %#" G_GINT64_MODIFIER "x", __func__,
										gst_toc_entry_type_get_nick(gst_toc_entry_get_entry_type(sub_entry)), y + 1, pts);
							}
						}
						y++;
					}
				}
				if (y > 0)
				{
					m_cuesheet_changed = 1;
					m_event((iPlayableService*)this, evCuesheetChanged);
				}
			}
		}
		//eDebug("[eServiceMP3] TOC entry from source %s processed", GST_MESSAGE_SRC_NAME(msg));
	}
	else
	{
		eDebug("[eServiceMP3::%s] TOC entry from source %s not used", __func__, GST_MESSAGE_SRC_NAME(msg));
	}
}

void eServiceMP3::playbinNotifySource(GObject *object, GParamSpec *unused, gpointer user_data)
{
	GstElement *source = NULL;
	eServiceMP3 *_this = (eServiceMP3*)user_data;
	g_object_get(object, "source", &source, NULL);
	if (source)
	{
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "timeout") != 0)
		{
			GstElementFactory *factory = gst_element_get_factory(source);
			if (factory)
			{
				const gchar *sourcename = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
				if (!strcmp(sourcename, "souphttpsrc"))
				{
					g_object_set(G_OBJECT(source), "timeout", HTTP_TIMEOUT, NULL);
				}
			}
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "ssl-strict") != 0)
		{
			g_object_set(G_OBJECT(source), "ssl-strict", FALSE, NULL);
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "user-agent") != 0 && !_this->m_useragent.empty())
		{
			g_object_set(G_OBJECT(source), "user-agent", _this->m_useragent.c_str(), NULL);
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "extra-headers") != 0 && !_this->m_extra_headers.empty())
		{
			GstStructure *extras = gst_structure_new_empty("extras");
			size_t pos = 0;
			while (pos != std::string::npos)
			{
				std::string name, value;
				size_t start = pos;
				size_t len = std::string::npos;
				pos = _this->m_extra_headers.find('=', pos);
				if (pos != std::string::npos)
				{
					len = pos - start;
					pos++;
					name = _this->m_extra_headers.substr(start, len);
					start = pos;
					len = std::string::npos;
					pos = _this->m_extra_headers.find('&', pos);
					if (pos != std::string::npos)
					{
						len = pos - start;
						pos++;
					}
					value = _this->m_extra_headers.substr(start, len);
				}
				if (!name.empty() && !value.empty())
				{
					GValue header;
					eDebug("[eServiceMP3::%s] setting extra-header '%s:%s'", __func__, name.c_str(), value.c_str());
					memset(&header, 0, sizeof(GValue));
					g_value_init(&header, G_TYPE_STRING);
					g_value_set_string(&header, value.c_str());
					gst_structure_set_value(extras, name.c_str(), &header);
				}
				else
				{
					eDebug("[eServiceMP3::%s] Invalid header format %s", __func__, _this->m_extra_headers.c_str());
					break;
				}
			}
			if (gst_structure_n_fields(extras) > 0)
			{
				g_object_set(G_OBJECT(source), "extra-headers", extras, NULL);
			}
			gst_structure_free(extras);
		}
		gst_object_unref(source);
	}
}

void eServiceMP3::handleElementAdded(GstBin *bin, GstElement *element, gpointer user_data)
{
	eServiceMP3 *_this = (eServiceMP3*)user_data;
	if (_this)
	{
		gchar *elementname = gst_element_get_name(element);

		if (g_str_has_prefix(elementname, "queue2"))
		{
			if (_this->m_download_buffer_path != "")
			{
				g_object_set(G_OBJECT(element), "temp-template", _this->m_download_buffer_path.c_str(), NULL);
			}
			else
			{
				g_object_set(G_OBJECT(element), "temp-template", NULL, NULL);
			}
		}
		else if (g_str_has_prefix(elementname, "uridecodebin")
		||       g_str_has_prefix(elementname, "decodebin"))
		{
			/*
			 * Listen for queue2 element added to uridecodebin/decodebin2 as well.
			 * Ignore other bins since they may have unrelated queues
			 */
			g_signal_connect(element, "element-added", G_CALLBACK(handleElementAdded), user_data);
		}
		g_free(elementname);
	}
}

audiotype_t eServiceMP3::gstCheckAudioPad(GstStructure* structure)
{
	if (!structure)
	{
		return atUnknown;
	}
	if (gst_structure_has_name(structure, "audio/mpeg"))
	{
		gint mpegversion, layer = -1;
		if (!gst_structure_get_int(structure, "mpegversion", &mpegversion))
		{
			return atUnknown;
		}
		switch (mpegversion)
		{
			case 1:
			{
				gst_structure_get_int(structure, "layer", &layer);
				return (layer == 3) ? atMP3 : atMPEG;
			}
			case 2:
			{
				return atAAC;
			}
			case 4:
			{
				return atAAC;
			}
			default:
			{
				return atUnknown;
			}
		}
	}
	else if (gst_structure_has_name(structure, "audio/x-ac3")
	||       gst_structure_has_name(structure, "audio/ac3"))
	{
		return atAC3;
	}
	else if (gst_structure_has_name(structure, "audio/x-dts")
	||       gst_structure_has_name(structure, "audio/dts"))
	{
		return atDTS;
	}
	else if (gst_structure_has_name(structure, "audio/x-raw"))
//	||       gst_structure_has_name(structure, "audio/x-raw-int"))
	{
		return atPCM;
	}
	return atUnknown;
}

void eServiceMP3::gstPoll(ePtr<GstMessageContainer> const &msg)
{
	switch (msg->getType())
	{
		case 1:
		{
			GstMessage *gstmessage = *((GstMessageContainer*)msg);
			if (gstmessage)
			{
				gstBusCall(gstmessage);
			}
			break;
		}
		case 2:
		{
			GstBuffer *buffer = *((GstMessageContainer*)msg);
			if (buffer)
			{
				pullSubtitle(buffer);
			}
			break;
		}
		case 3:
		{
			GstPad *pad = *((GstMessageContainer*)msg);
			gstTextpadHasCAPS_synced(pad);
			break;
		}
	}
}
#endif //GSTREAMER

eAutoInitPtr<eServiceFactoryMP3> init_eServiceFactoryMP3(eAutoInitNumbers::service+1, "eServiceFactoryMP3");

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
void eServiceMP3::gstCBsubtitleAvail(GstElement *subsink, GstBuffer *buffer, gpointer user_data)
{
	eServiceMP3 *_this = (eServiceMP3*)user_data;
	if (_this->m_currentSubtitleStream < 0)
	{
		if (buffer)
		{
			gst_buffer_unref(buffer);
		}
		return;
	}
	_this->m_pump.send(new GstMessageContainer(2, NULL, NULL, buffer));
}

void eServiceMP3::gstTextpadHasCAPS(GstPad *pad, GParamSpec * unused, gpointer user_data)
{
	eServiceMP3 *_this = (eServiceMP3*)user_data;

	gst_object_ref(pad);

	_this->m_pump.send(new GstMessageContainer(3, NULL, pad, NULL));
}

void eServiceMP3::gstTextpadHasCAPS_synced(GstPad *pad)
{
	GstCaps *caps = NULL;

	g_object_get(G_OBJECT(pad), "caps", &caps, NULL);

	if (caps)
	{
		subtitleStream subs;

		eDebug("[eServiceMP3::%s] signal::caps = %s", __func__, gst_caps_to_string(caps));
//		eDebug("[eServiceMP3::%s] pad = %p  size = %d", pad, m_subtitleStreams.size());

		if (m_currentSubtitleStream >= 0 && m_currentSubtitleStream < (int)m_subtitleStreams.size())
		{
			subs = m_subtitleStreams[m_currentSubtitleStream];
		}
		else
		{
			subs.type = stUnknown;
			subs.pad = pad;
		}
		if (subs.type == stUnknown)
		{
			GstTagList *tags = NULL;
			gchar *g_lang = NULL;
			g_signal_emit_by_name(m_gst_playbin, "get-text-tags", m_currentSubtitleStream, &tags);

			subs.language_code = "und";
			subs.type = getSubtitleType(pad);
			if (tags && GST_IS_TAG_LIST(tags))
			{
				if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &g_lang))
				{
					subs.language_code = std::string(g_lang);
					g_free(g_lang);
				}
				gst_tag_list_free(tags);
			}
			if (m_currentSubtitleStream >= 0 && m_currentSubtitleStream < (int)m_subtitleStreams.size())
			{
				m_subtitleStreams[m_currentSubtitleStream] = subs;
			}
			else
			{
				m_subtitleStreams.push_back(subs);
			}
		}
//		eDebug("[eServiceMP3::%s] m_gst_prev_subtitle_caps=%s equal=%i", __func__, gst_caps_to_string(m_gst_prev_subtitle_caps), gst_caps_is_equal(m_gst_prev_subtitle_caps, caps));

		gst_caps_unref(caps);
	}
}

// SPU decoder adapted from DreamDVD by Seddi & Mirakels.

static int vobsub_pal[16], vobsub_col[4], vobsub_a[4];

typedef struct ddvd_spudec_clut_struct
{
#if BYTE_ORDER == BIG_ENDIAN
	uint8_t e2 : 4;
	uint8_t e1 : 4;
	uint8_t p  : 4;
	uint8_t b  : 4;
#else
	uint8_t e1 : 4;
	uint8_t e2 : 4;
	uint8_t b  : 4;
	uint8_t p  : 4;
#endif
} ddvd_spudec_clut_t;

static void get_vobsub_palette(GstElement *playbin, int subtitle_stream)
{
	GstPad* pad = 0;
	g_signal_emit_by_name(playbin, "get-text-pad", subtitle_stream, &pad);
	if (pad)
	{
		GstCaps *caps = NULL;
		g_object_get(G_OBJECT(pad), "caps", &caps, NULL);
		GstStructure *s = gst_caps_get_structure(caps, 0);
		const GValue *val = gst_structure_get_value(s, "codec_data");
		if (val)
		{
			GstBuffer *buffer = (GstBuffer *) g_value_get_boxed(val);
			guint8 *data;
			gsize size;
			GstMapInfo map;
			gst_buffer_map(buffer, &map, GST_MAP_READ);
			data = map.data;
			size = map.size;
			std::string idx((const char*)data, size);
			gst_buffer_unmap(buffer, &map);
			const char *palette = strstr(idx.c_str(), "palette:");
			if (palette)
			{
				palette += 8;
				int i = 0, len;
				while (i < 16 && sscanf(palette, "%x%n", &vobsub_pal[i], &len) == 1)
				{
					palette += len;
					if (*palette == ',')
					{
						++palette;
					}
					++i;
				}
			}
		}
	}
}

// SPU Decoder (reference: http://stnsoft.com/DVD/spu.html)
static ePtr<gPixmap> ddvd_spu_decode_data(const uint8_t *buffer, size_t bufsize, int &display_time)
{
	int x = 0, sx = 0, ex = 0, sy = 576, ey = 575;
	int offset[2], param_len;
	int size, dcsq, aligned, id;

	offset[0] = display_time = -1;

	size = buffer[0] << 8 | buffer[1];
	dcsq = buffer[2] << 8 | buffer[3];

	if ((size_t)size > bufsize || dcsq > size)
	{
		return 0;
	}
	// parse header
	int i = dcsq + 4;

	while (i < size && buffer[i] != 0xFF)
	{
		switch (buffer[i])
		{
			case 0x00:	// force
			{
				// force_hide = SPU_FORCE; // Highlight mask SPU
				i++;
				break;
			}
			case 0x01:	// show
			{
				// force_hide = SPU_SHOW; // Subtitle SPU
				i++;
				break;
			}
			case 0x02:	// hide
			{
				// force_hide = SPU_HIDE; // Probably only as second control block in Subtitle SPU. See scan for display_time below
				i++;
				break;
			}
			case 0x03:	// palette
			{
				ddvd_spudec_clut_t *clut = (ddvd_spudec_clut_t *)(buffer + i + 1);

				vobsub_col[0] = vobsub_pal[clut->b];
				vobsub_col[1] = vobsub_pal[clut->p];
				vobsub_col[2] = vobsub_pal[clut->e1];
				vobsub_col[3] = vobsub_pal[clut->e2];

				i += 3;
				break;
			}
			case 0x04:	// transparency palette
			{
				ddvd_spudec_clut_t *clut = (ddvd_spudec_clut_t *)(buffer + i + 1);

				vobsub_a[0] = clut->b * 0x11;
				vobsub_a[1] = clut->p * 0x11;
				vobsub_a[2] = clut->e1 * 0x11;
				vobsub_a[3] = clut->e2 * 0x11;

				i += 3;
				break;
			}
			case 0x05:	// image coordinates
			{
				x =
				sx = buffer[i + 1] << 4 | buffer[i + 2] >> 4;
				sy = buffer[i + 4] << 4 | buffer[i + 5] >> 4;
				ex = (buffer[i + 2] & 0x0f) << 8 | buffer[i + 3];
				ey = (buffer[i + 5] & 0x0f) << 8 | buffer[i + 6];
				if (ex > 719)
				{
					ex = 719;
				}
				if (ey > 575)
				{
					ey = 575;
				}
				i += 7;
				break;
			}
			case 0x06:	// image 1 / image 2 offsets
			{
				offset[0] = buffer[i + 1] << 8 | buffer[i + 2];
				offset[1] = buffer[i + 3] << 8 | buffer[i + 4];
				i += 5;
				break;
			}
			case 0x07:	// change color for a special area so overlays with more than 4 colors are possible - NOT IMPLEMENTED YET
			{
				param_len = buffer[i + 1] << 8 | buffer[i + 2];
				i += param_len + 1;
				break;
			}
			default:
			{
				i++;
				break;
			}
		}
	}
	// get display time - actually a plain control block
	if (i + 6 <= size && buffer[i + 5] == 0x02 && buffer[i + 6] == 0xFF)
	{
		display_time = (buffer[i + 1] << 8 | buffer[i + 2]) * 1024 / 90;
	}
	if (sy == 576 || offset[0] == -1 || display_time == -1)
	{
		return 0;
	}
	ePtr<gPixmap> pixmap = new gPixmap(eSize(720, 576), 32);
	uint32_t *spu_buf = (uint32_t *)pixmap->surface->data;
	memset(spu_buf, 0, 720 * 576 * 4);

	// parse picture
	aligned = 1;
	id = 0;

	while (sy <= ey)
	{
		u_int len;
		u_int code;

		code = (aligned ? (buffer[offset[id]++] >> 4) : (buffer[offset[id] - 1] & 0xF));
		aligned ^= 1;

		if (code < 0x0004)
		{
			code = (code << 4) | (aligned ? (buffer[offset[id]++] >> 4) : (buffer[offset[id] - 1] & 0xF));
			aligned ^= 1;
			if (code < 0x0010)
			{
				code = (code << 4) | (aligned ? (buffer[offset[id]++] >> 4) : (buffer[offset[id] - 1] & 0xF));
				aligned ^= 1;
				if (code < 0x0040)
				{
					code = (code << 4) | (aligned ? (buffer[offset[id]++] >> 4) : (buffer[offset[id] - 1] & 0xF));
					aligned ^= 1;
				}
			}
		}
		len = code >> 2;
		if (len == 0)
		{
			len = ex - x + 1;
		}
		int p = code & 3;
		int a = vobsub_a[p];
		if (a != 0)
		{
			uint32_t c = a << 24 | vobsub_col[p];
			uint32_t *dst = spu_buf + sy * 720 + x, *end = dst + len;
			do
			{
				*dst++ = c;
			}
			while (dst != end);
		}
		x += len;
		if (x > ex)
		{
			x = sx;  // next line
			sy++;
			aligned = 1;
			id ^= 1;
		}
	}
	return pixmap;
}

void eServiceMP3::pullSubtitle(GstBuffer *buffer)
{
	if (buffer && m_currentSubtitleStream >= 0 && m_currentSubtitleStream < (int)m_subtitleStreams.size())
	{
		GstMapInfo map;
		if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
		{
			eLog(3, "[eServiceMP3::%s] gst_buffer_map failed", __func__);
			return;
		}
		gint64 buf_pos = GST_BUFFER_PTS(buffer);
		size_t len = map.size;
		eLog(6, "[eServiceMP3::%s] gst_buffer_get_size %zu map.size %zu", __func__, gst_buffer_get_size(buffer), len);
		gint64 duration_ns = GST_BUFFER_DURATION(buffer);
		int subType = m_subtitleStreams[m_currentSubtitleStream].type;
		eLog(6, "[eServiceMP3::%s] type=%d size=%zu", __func__, subType, len);
		if (subType)
		{
			if (subType <= stVOB)
			{
				int delay = eConfigManager::getConfigIntValue("config.subtitles.pango_subtitles_delay");
				int subtitle_fps = eConfigManager::getConfigIntValue("config.subtitles.pango_subtitles_fps");

				double convert_fps = 1.0;
				if (subtitle_fps > 1 && m_framerate > 0)
				{
					convert_fps = subtitle_fps / (double)m_framerate;
				}
				uint32_t start_ms = ((buf_pos / 1000000ULL) * convert_fps) + (delay / 90);
				uint32_t end_ms = start_ms + (duration_ns / 1000000ULL);
				if (subType == stVOB)
				{
					int display_time;

					ePtr<gPixmap> pixmap = ddvd_spu_decode_data((const uint8_t*)map.data, len, display_time);
					if (pixmap)
					{
						end_ms = start_ms + display_time;
						eLog(6, "[eServiceMP3::%s] got new pic subtitle @ buf_pos = %lld ns (in pts=%lld), dur=%d ms", __func__, buf_pos, buf_pos/11111, display_time);
						m_subtitle_pages.insert(subtitle_pages_map_pair_t(end_ms, subtitle_page_t(start_ms, end_ms, pixmap)));
					}
					else
					{
						eLog(6, "[eServiceMP3::%s] failed to decode SPU @ buf_pos = %lld ns (in pts=%lld)", __func__, buf_pos, buf_pos/11111);
					}
				}
				else
				{
					std::string line((const char*)map.data, len);
					eLog(6, "[eServiceMP3::%s] got new text subtitle @ buf_pos = %lld ns (in pts=%lld), dur=%lld: '%s' ", __func__, buf_pos, buf_pos/11111, duration_ns, line.c_str());
					m_subtitle_pages.insert(subtitle_pages_map_pair_t(end_ms, subtitle_page_t(start_ms, end_ms, line)));
				}
				m_subtitle_sync_timer->start(1, true);
			}
			else
			{
				eLog(3, "[eServiceMP3::%s] Unsupported subpicture... ignoring", __func__);
			}
		}
		gst_buffer_unmap(buffer, &map);
	}
}
#else // eplayer3
void eServiceMP3::eplayerCBsubtitleAvail(long int duration_ms, size_t len, char * buffer, void* user_data)
{
	eDebug("[eServiceMP3::%s] >", __func__);
	unsigned char tmp[len + 1];

	memcpy(tmp, buffer, len);
	tmp[len] = 0;
	eDebug("[eServiceMP3::%s] gstCBsubtitleAvail: %s", __func__, tmp);
	eServiceMP3 *_this = (eServiceMP3*)user_data;

	if (_this->m_subtitle_widget)
	{
		ePangoSubtitlePage page;
		gRGB rgbcol(0xD0, 0xD0, 0xD0);
		page.m_elements.push_back(ePangoSubtitlePageElement(rgbcol, (const char*)tmp));
		page.m_timeout = duration_ms;
		(_this->m_subtitle_widget)->setPage(page);
	}
	eDebug("[eServiceMP3::%s] <", __func__);
}
#endif

void eServiceMP3::pushSubtitles()
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	pts_t running_pts = 0;
	int32_t next_timer = 0, decoder_ms, start_ms, end_ms, diff_start_ms, diff_end_ms;
	subtitle_pages_map_t::iterator current;

	m_decoder_time_valid_state = 4;
	// wait until clock is stable

	if (getPlayPosition(running_pts) < 0)
	{
		m_decoder_time_valid_state = 0;
	}
	else
	{
		m_prev_decoder_time = running_pts;
	}
	if (m_decoder_time_valid_state < 4)
	{
		m_decoder_time_valid_state++;

		if (m_prev_decoder_time == running_pts && !m_paused)
		{
			m_decoder_time_valid_state = 1;
		}
		if (m_decoder_time_valid_state < 4)
		{
//			eDebug("[eServiceMP3::%s] Waiting for clock to stabilise", __func__);
			m_prev_decoder_time = running_pts;
			next_timer = 100;
			goto exit;
		}
//		eDebug("[eServiceMP3::%s] Clock stable", __func__);
	}
	decoder_ms = running_pts / 90;

#if 0
	eDebug("[eServiceMP3::%s] *** all subs: ", __func__);

	for (current = m_subtitle_pages.begin(); current != m_subtitle_pages.end(); current++)
	{
		start_ms = current->second.start_ms;
		end_ms = current->second.end_ms;
		diff_start_ms = start_ms - decoder_ms;
		diff_end_ms = end_ms - decoder_ms;

		eDebug("[eServiceMP3::%s]    start: %d, end: %d, diff_start: %d, diff_end: %d: %s",
					__func__, start_ms, end_ms, diff_start_ms, diff_end_ms, current->second.text.c_str());
	}
#endif

	for (current = m_subtitle_pages.lower_bound(decoder_ms); current != m_subtitle_pages.end(); current++)
	{
		start_ms = current->second.start_ms;
		end_ms = current->second.end_ms;
		diff_start_ms = start_ms - decoder_ms;
		diff_end_ms = end_ms - decoder_ms;

#if 0
		eDebug("[eServiceMP3::%s] *** next subtitle: decoder: %d, start: %d, end: %d, duration_ms: %d, diff_start: %d, diff_end: %d : %s",
			__func__, decoder_ms, start_ms, end_ms, end_ms - start_ms, diff_start_ms, diff_end_ms, current->second.text.c_str());
#endif

		if (diff_end_ms < 0)
		{
//			eDebug("[eServiceMP3::%s] Current sub has already ended, skip: %d", __func__, diff_end_ms);
			continue;
		}

		if (diff_start_ms > 20)
		{
//			eDebug("[eServiceMP3::%s] Current sub in the future, start timer, %d", __func__, diff_start_ms);
			next_timer = diff_start_ms;
			goto exit;
		}

		// showtime
		if (m_subtitle_widget && !m_paused)
		{
			int timeout;

//			eDebug("[eServiceMP3::%s] Current sub actual, show!", __func__);

			if (!m_subtitles_paused)
			{
				timeout = end_ms - decoder_ms;	// take late start into account
			}
			else
			{
				timeout = 60000;  // paused, subs must stay on (60s for now), avoid timeout in lib/gui/esubtitle.cpp: m_hide_subtitles_timer->start(m_pango_page.m_timeout, true);
			}
			if (current->second.pixmap)
			{
				eVobSubtitlePage vobsub_page;
				vobsub_page.m_show_pts = start_ms * 90;  // actually completely unused by widget!
				vobsub_page.m_timeout = timeout;
				vobsub_page.m_pixmap = current->second.pixmap;

//				m_subtitle_widget->setPage(vobsub_page);
			}
			else
			{
				ePangoSubtitlePage pango_page;
				gRGB rgbcol(0xD0, 0xD0, 0xD0);

				pango_page.m_elements.push_back(ePangoSubtitlePageElement(rgbcol, current->second.text.c_str()));
				pango_page.m_show_pts = start_ms * 90;  // actually completely unused by widget!
				pango_page.m_timeout = timeout;

				m_subtitle_widget->setPage(pango_page);
			}
		}
	}
	// no more subs in cache, fall through

exit:
	if (next_timer == 0)
	{
//		eDebug("[eServiceMP3::%s] Next timer = 0, set default timer!", __func__);
		next_timer = 1000;
	}

	m_subtitle_sync_timer->start(next_timer, true);

#endif
}

RESULT eServiceMP3::enableSubtitles(iSubtitleUser *user, struct SubtitleTrack &track)
{
	if (m_currentSubtitleStream != track.pid)
	{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
		g_object_set(m_gst_playbin, "current-text", -1, NULL);
#endif
		m_subtitle_sync_timer->stop();
		m_subtitle_pages.clear();
		m_prev_decoder_time = -1;
		m_decoder_time_valid_state = 0;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
		m_currentSubtitleStream = track.pid;
		m_cachedSubtitleStream = m_currentSubtitleStream;
		if (m_subtitle_widget)
		{
			m_subtitle_widget->destroy();
		}
		g_object_set(m_gst_playbin, "current-text", m_currentSubtitleStream, NULL);
#endif
		m_subtitle_widget = user;

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
		eDebug("[eServiceMP3] switched to subtitle stream %i", m_currentSubtitleStream);

		if (track.page_number == stVOB)
		{
			get_vobsub_palette(m_gst_playbin, m_currentSubtitleStream);
		}
#ifdef GSTREAMER_SUBTITLE_SYNC_MODE_BUG
		/*
		 * When we're running the subsink in sync=false mode,
		 * we have to force a seek, before the new subtitle stream will start.
		 */
		seekRelative(-1, 90000);
#endif
#else  // eplayer3
		m_currentSubtitleStream = track.pid;
		m_cachedSubtitleStream = m_currentSubtitleStream;
		m_subtitle_widget = user;
		
		eDebug("[eServiceMP3::%s] switched to subtitle stream %i, type %d", __func__, m_currentSubtitleStream, track.page_number);

		if (track.page_number > 1)
		{
			pullTextSubtitles(track.page_number);
		}
		else
		{
			m_emb_subtitle_pages.clear();
			m_subtitle_pages = &m_emb_subtitle_pages;
			player->SwitchSubtitle(track.magazine_number);
			seekRelative(-1, 5000);
		}
#endif
	}
	return 0;
}

RESULT eServiceMP3::disableSubtitles()
{
	eDebug("[eServiceMP3::%s] >", __func__);

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	m_currentSubtitleStream = -1;
	m_cachedSubtitleStream = m_currentSubtitleStream;
	g_object_set(m_gst_playbin, "current-text", m_currentSubtitleStream, NULL);
#endif
	m_subtitle_sync_timer->stop();
	m_subtitle_pages.clear();
	m_prev_decoder_time = -1;
	m_decoder_time_valid_state = 0;
	if (m_subtitle_widget)
	{
		m_subtitle_widget->destroy();
	}
	m_subtitle_widget = 0;
#if !defined ENABLE_GSTREAMER \
 && !defined ENABLE_DUAL_MEDIAFW
// eplayer3
	if (m_state != stRunning)
	{
		return 0;
	}
	player->SwitchSubtitle(-1);
#endif
	return 0;
}

RESULT eServiceMP3::getCachedSubtitle(struct SubtitleTrack &track)
{

	bool autoturnon = eConfigManager::getConfigBoolValue("config.subtitles.pango_autoturnon", true);
	int m_subtitleStreams_size = (int)m_subtitleStreams.size();
	if (!autoturnon)
	{
		return -1;
	}
	if (m_cachedSubtitleStream == -2 && m_subtitleStreams_size)
	{
		m_cachedSubtitleStream = 0;
		int autosub_level = 5;
		std::string configvalue;
		std::vector<std::string> autosub_languages;
		configvalue = eConfigManager::getConfigValue("config.autolanguage.subtitle_autoselect1");
		if (configvalue != "" && configvalue != "None")
		{
			autosub_languages.push_back(configvalue);
		}
		configvalue = eConfigManager::getConfigValue("config.autolanguage.subtitle_autoselect2");
		if (configvalue != "" && configvalue != "None")
		{
			autosub_languages.push_back(configvalue);
		}
		configvalue = eConfigManager::getConfigValue("config.autolanguage.subtitle_autoselect3");
		if (configvalue != "" && configvalue != "None")
		{
			autosub_languages.push_back(configvalue);
		}
		configvalue = eConfigManager::getConfigValue("config.autolanguage.subtitle_autoselect4");
		if (configvalue != "" && configvalue != "None")
		{
			autosub_languages.push_back(configvalue);
		}
		for (int i = 0; i < m_subtitleStreams_size; i++)
		{
			if (!m_subtitleStreams[i].language_code.empty())
			{
				int x = 1;
				for (std::vector<std::string>::iterator it2 = autosub_languages.begin(); x < autosub_level && it2 != autosub_languages.end(); x++, it2++)
				{
					if ((*it2).find(m_subtitleStreams[i].language_code) != std::string::npos)
					{
						autosub_level = x;
						m_cachedSubtitleStream = i;
						break;
					}
				}
			}
		}
	}

	if (m_cachedSubtitleStream >= 0 && m_cachedSubtitleStream < m_subtitleStreams_size)
	{
		track.type = 2;
		track.pid = m_cachedSubtitleStream;
		track.page_number = int(m_subtitleStreams[m_cachedSubtitleStream].type);
		track.magazine_number = 0;
		track.language_code = m_subtitleStreams[m_cachedSubtitleStream].language_code;
		return 0;
	}
	return -1;
}

RESULT eServiceMP3::getSubtitleList(std::vector<struct SubtitleTrack> &subtitlelist)
{
// 	eDebug("[eServiceMP3::%s] >", __func__);
	int stream_idx = 0;

	for (std::vector<subtitleStream>::iterator IterSubtitleStream(m_subtitleStreams.begin()); IterSubtitleStream != m_subtitleStreams.end(); ++IterSubtitleStream)
	{
		subtype_t type = IterSubtitleStream->type;
		switch (type)
		{
			case stUnknown:
			case stPGS:
			{
				break;
			}
			default:
			{
				struct SubtitleTrack track;
				track.type = 2;
				track.pid = stream_idx;
				track.page_number = int(type);
				track.magazine_number = 0;
				track.language_code = IterSubtitleStream->language_code;
				subtitlelist.push_back(track);
			}
		}
		stream_idx++;
	}
	eDebug("[eServiceMP3::%s] <", __func__);
	return 0;
}

RESULT eServiceMP3::streamed(ePtr<iStreamedService> &ptr)
{
	ptr = this;
	return 0;
}

ePtr<iStreamBufferInfo> eServiceMP3::getBufferCharge()
{
	return new eStreamBufferInfo(m_bufferInfo.bufferPercent, m_bufferInfo.avgInRate, m_bufferInfo.avgOutRate, m_bufferInfo.bufferingLeft, m_buffer_size);
}
/* cuesheet CVR */
PyObject *eServiceMP3::getCutList()
{
	ePyObject list = PyList_New(0);

	for (std::multiset<struct cueEntry>::iterator i(m_cue_entries.begin()); i != m_cue_entries.end(); ++i)
	{
		ePyObject tuple = PyTuple_New(2);
		PyTuple_SET_ITEM(tuple, 0, PyLong_FromLongLong(i->where));
		PyTuple_SET_ITEM(tuple, 1, PyInt_FromLong(i->what));
		PyList_Append(list, tuple);
		Py_DECREF(tuple);
	}
	return list;
}

/* cuesheet CVR */
void eServiceMP3::setCutList(ePyObject list)
{
	if (!PyList_Check(list))
	{
		return;
	}
	int size = PyList_Size(list);
	int i;

	m_cue_entries.clear();

	for (i = 0; i < size; ++i)
	{
		ePyObject tuple = PyList_GET_ITEM(list, i);
		if (!PyTuple_Check(tuple))
		{
			eDebug("[eServiceMP3::%s] non-tuple in cutlist", __func__);
			continue;
		}
		if (PyTuple_Size(tuple) != 2)
		{
			eDebug("[eServiceMP3::%s] cutlist entries need to be a 2-tuple", __func__);
			continue;
		}
		ePyObject ppts = PyTuple_GET_ITEM(tuple, 0), ptype = PyTuple_GET_ITEM(tuple, 1);
		if (!(PyLong_Check(ppts) && PyInt_Check(ptype)))
		{
			eDebug("[eServiceMP3::%s] cutlist entries need to be (pts, type)-tuples (%d %d)", __func__, PyLong_Check(ppts), PyInt_Check(ptype));
			continue;
		}
		pts_t pts = PyLong_AsLongLong(ppts);
		int type = PyInt_AsLong(ptype);
		m_cue_entries.insert(cueEntry(pts, type));
		eDebug("[eServiceMP3::%s] adding %" G_GINT64_FORMAT " type %d", __func__, (gint64)pts, type);
	}
	m_cuesheet_changed = 1;
	m_event((iPlayableService*)this, evCuesheetChanged);
}

void eServiceMP3::setCutListEnable(int enable)
{
	m_cutlist_enabled = enable;
}

int eServiceMP3::setBufferSize(int size)
{
	m_buffer_size = size;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	g_object_set(m_gst_playbin, "buffer-size", m_buffer_size, NULL);
#endif
	return 0;
}

int eServiceMP3::getAC3Delay()
{
	return ac3_delay;
}

int eServiceMP3::getPCMDelay()
{
	return pcm_delay;
}

void eServiceMP3::setAC3Delay(int delay)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	ac3_delay = delay;
	if (!m_gst_playbin || m_state != stRunning)
	{
		return;
	}
	else
	{
		int config_delay_int = delay;
		/*
		 * NOTE: We only look for dvbmediasinks.
		 * If either the video or audio sink is of a different type,
		 * we have no chance to get them synced anyway.
		 */
		if (dvb_videosink)
		{
			config_delay_int += eConfigManager::getConfigIntValue("config.av.generalAC3delay");
		}
		else
		{
			eDebug("[eServiceMP3::%s] do not apply AC3 delay if no video is running!", __func__);
			config_delay_int = 0;
		}

		if (dvb_audiosink)
		{
			eTSMPEGDecoder::setHwAC3Delay(config_delay_int);
		}
	}
#endif
}

void eServiceMP3::setPCMDelay(int delay)
{
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	pcm_delay = delay;
	if (!m_gst_playbin || m_state != stRunning)
	{
		return;
	}
	else
	{
		int config_delay_int = delay;
		/*
		 * NOTE: We only look for dvbmediasinks.
		 * If either the video or audio sink is of a different type,
		 * we have no chance to get them synced anyway.
		 */
		if (dvb_videosink)
		{
			config_delay_int += eConfigManager::getConfigIntValue("config.av.generalPCMdelay");
		}
		else
		{
			eDebug("[eServiceMP3::%s] do not apply PCM delay if no video is running!", __func__);
			config_delay_int = 0;
		}

		if (dvb_audiosink)
		{
			eTSMPEGDecoder::setHwPCMDelay(config_delay_int);
		}
	}
#endif
}

/* cuesheet CVR */
void eServiceMP3::loadCuesheet()
{
	if (!m_cuesheet_loaded)
	{
		eDebug("[eServiceMP3::%s] >", __func__);
		m_cuesheet_loaded = true;
	}
	else
	{
		eDebug("[eServiceMP3::%s] skip loading cuesheet multiple times", __func__);
		return;
	}
 
	m_cue_entries.clear();
	/* only load manual cuts if no chapter info avbl CVR */
	if (m_use_chapter_entries)
	{
		return;
	}
	std::string filename = m_ref.path + ".cuts";

	m_cue_entries.clear();

	FILE *f = fopen(filename.c_str(), "rb");

	if (f)
	{
		while (1)
		{
			unsigned long long where;
			unsigned int what;

			if (!fread(&where, sizeof(where), 1, f))
			{
				break;
			}
			if (!fread(&what, sizeof(what), 1, f))
			{
				break;
			}
			where = be64toh(where);
			what = ntohl(what);

			if (what > 3)
			{
				break;
			}
			m_cue_entries.insert(cueEntry(where, what));
		}
		fclose(f);
		eDebug("[eServiceMP3::%s] cuts file has %zd entries", __func__, m_cue_entries.size());
	}
	else
	{
		eDebug("[eServiceMP3::%s] no cutfile found", __func__);
	}
	m_cuesheet_changed = 0;
	m_event((iPlayableService*)this, evCuesheetChanged);
}

/* cuesheet CVR */
void eServiceMP3::saveCuesheet()
{
	std::string filename = m_ref.path;

	/* save cuesheet only when main file is accessible. and no TOC chapters avbl*/
	if ((::access(filename.c_str(), R_OK) < 0) || m_use_chapter_entries)
	{
		return;
	}
	filename.append(".cuts");
	/* do not save to file if there are no cuts */
	/* remove the cuts file if cue is empty */
	if (m_cue_entries.begin() == m_cue_entries.end())
	{
		if (::access(filename.c_str(), F_OK) == 0)
		{
			remove(filename.c_str());
		}
		return;
	}
	FILE *f = fopen(filename.c_str(), "wb");

	if (f)
	{
		unsigned long long where;
		int what;

		for (std::multiset<cueEntry>::iterator i(m_cue_entries.begin()); i != m_cue_entries.end(); ++i)
		{
			where = htobe64(i->where);
			what = htonl(i->what);
			fwrite(&where, sizeof(where), 1, f);
			fwrite(&what, sizeof(what), 1, f);
		}
		fclose(f);
	}
	m_cuesheet_changed = 0;
}

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
__attribute__((constructor)) void libraryinit(int argc, char **argv)
{
	gst_init(NULL, NULL);
}
#endif
// vim:ts=4
