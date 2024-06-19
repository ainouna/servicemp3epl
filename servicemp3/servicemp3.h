#ifndef __servicemp3_h
#define __servicemp3_h

#include <lib/base/message.h>
#include <lib/service/iservice.h>
#include <lib/dvb/pmt.h>
#include <lib/dvb/subtitle.h>
#include <lib/dvb/teletext.h>
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
#include <gst/gst.h>
#else
#include <common.h>
#include <subtitle.h>
#define gint int
#define gint64 int64_t
extern OutputHandler_t		OutputHandler;
extern PlaybackHandler_t	PlaybackHandler;
extern ContainerHandler_t	ContainerHandler;
extern ManagerHandler_t	ManagerHandler;
#endif

/* for subtitles */
#include <lib/gui/esubtitle.h>

class eStaticServiceMP3Info;

class eServiceFactoryMP3: public iServiceHandler
{
	DECLARE_REF(eServiceFactoryMP3);
public:
	eServiceFactoryMP3();
	virtual ~eServiceFactoryMP3();
#if defined ENABLE_DUAL_MEDIAFW
	enum {
		id = 4097,
		idServiceMP3 = 5001
	};
#else
	enum { id = eServiceReference::idServiceMP3 };
#endif
		// iServiceHandler
	RESULT play(const eServiceReference &, ePtr<iPlayableService> &ptr);
	RESULT record(const eServiceReference &, ePtr<iRecordableService> &ptr);
	RESULT list(const eServiceReference &, ePtr<iListableService> &ptr);
	RESULT info(const eServiceReference &, ePtr<iStaticServiceInformation> &ptr);
	RESULT offlineOperations(const eServiceReference &, ePtr<iServiceOfflineOperations> &ptr);
private:
	ePtr<eStaticServiceMP3Info> m_service_info;
#if defined ENABLE_DUAL_MEDIAFW
	bool defaultMP3Player;
#endif
};

class eStaticServiceMP3Info: public iStaticServiceInformation
{
	DECLARE_REF(eStaticServiceMP3Info);
	friend class eServiceFactoryMP3;
	eStaticServiceMP3Info();
public:
	RESULT getName(const eServiceReference &ref, std::string &name);
	int getLength(const eServiceReference &ref);
	int getInfo(const eServiceReference &ref, int w);
	int isPlayable(const eServiceReference &ref, const eServiceReference &ignore, bool simulate) { return 1; }
	long long getFileSize(const eServiceReference &ref);
	RESULT getEvent(const eServiceReference &ref, ePtr<eServiceEvent> &ptr, time_t start_time);
};

class eStreamBufferInfo: public iStreamBufferInfo
{
	DECLARE_REF(eStreamBufferInfo);
	int bufferPercentage;
	int inputRate;
	int outputRate;
	int bufferSpace;
	int bufferSize;

public:
	eStreamBufferInfo(int percentage, int inputrate, int outputrate, int space, int size);

	int getBufferPercentage() const;
	int getAverageInputRate() const;
	int getAverageOutputRate() const;
	int getBufferSpace() const;
	int getBufferSize() const;
};

class eServiceMP3InfoContainer: public iServiceInfoContainer
{
	DECLARE_REF(eServiceMP3InfoContainer);

	double doubleValue;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW

	GstBuffer *bufferValue;
#endif

	unsigned char *bufferData;
	unsigned int bufferSize;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	GstMapInfo map;
#endif

public:
	eServiceMP3InfoContainer();
	~eServiceMP3InfoContainer();

	double getDouble(unsigned int index) const;
	unsigned char *getBuffer(unsigned int &size) const;
	void setDouble(double value);
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	void setBuffer(GstBuffer *buffer);
#endif
};

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
class GstMessageContainer: public iObject
{
	DECLARE_REF(GstMessageContainer);
	GstMessage *messagePointer;
	GstPad *messagePad;
	GstBuffer *messageBuffer;
	int messageType;

public:
	GstMessageContainer(int type, GstMessage *msg, GstPad *pad, GstBuffer *buffer)
	{
		messagePointer = msg;
		messagePad = pad;
		messageBuffer = buffer;
		messageType = type;
	}
	~GstMessageContainer()
	{
		if (messagePointer) gst_message_unref(messagePointer);
		if (messagePad) gst_object_unref(messagePad);
		if (messageBuffer) gst_buffer_unref(messageBuffer);
	}
	int getType() { return messageType; }
	operator GstMessage *() { return messagePointer; }
	operator GstPad *() { return messagePad; }
	operator GstBuffer *() { return messageBuffer; }
};

typedef struct _GstElement GstElement;
#endif

typedef enum { atUnknown, atMPEG, atMP3, atAC3, atDTS, atAAC, atPCM, atOGG, atFLAC, atWMA } audiotype_t;
typedef enum { stUnknown, stPlainText, stSSA, stASS, stSRT, stVOB, stPGS } subtype_t;
typedef enum { ctNone, ctMPEGTS, ctMPEGPS, ctMKV, ctAVI, ctMP4, ctVCD, ctCDA, ctASF, ctOGG, ctWEBM } containertype_t;

class eServiceMP3: public iPlayableService, public iPauseableService,
	public iServiceInformation, public iSeekableService, public iAudioTrackSelection, public iAudioChannelSelection,
	public iSubtitleOutput, public iStreamedService, public iAudioDelay, public sigc::trackable, public iCueSheet
{
	DECLARE_REF(eServiceMP3);
public:
	virtual ~eServiceMP3();

		// iPlayableService
	RESULT connectEvent(const sigc::slot2<void,iPlayableService*,int> &event, ePtr<eConnection> &connection);
	RESULT start();
	RESULT stop();

	RESULT pause(ePtr<iPauseableService> &ptr);
	RESULT setSlowMotion(int ratio);
	RESULT setFastForward(int ratio);

	RESULT seek(ePtr<iSeekableService> &ptr);
	RESULT audioTracks(ePtr<iAudioTrackSelection> &ptr);
	RESULT audioChannel(ePtr<iAudioChannelSelection> &ptr);
	RESULT subtitle(ePtr<iSubtitleOutput> &ptr);
	RESULT audioDelay(ePtr<iAudioDelay> &ptr);
	RESULT cueSheet(ePtr<iCueSheet> &ptr);

		// not implemented (yet)
	RESULT setTarget(int target, bool noaudio = false) { return -1; }
	RESULT frontendInfo(ePtr<iFrontendInformation> &ptr) { ptr = 0; return -1; }
	RESULT subServices(ePtr<iSubserviceList> &ptr) { ptr = 0; return -1; }
	RESULT timeshift(ePtr<iTimeshiftService> &ptr) { ptr = 0; return -1; }

		// iCueSheet
	PyObject *getCutList();
	void setCutList(SWIG_PYOBJECT(ePyObject));
	void setCutListEnable(int enable);

	RESULT rdsDecoder(ePtr<iRdsDecoder> &ptr) { ptr = 0; return -1; }
	RESULT keys(ePtr<iServiceKeys> &ptr) { ptr = 0; return -1; }
	RESULT stream(ePtr<iStreamableService> &ptr) { ptr = 0; return -1; }

	void setQpipMode(bool value, bool audio) { }

		// iPausableService
	RESULT pause();
	RESULT unpause();

	RESULT info(ePtr<iServiceInformation>&);

		// iSeekableService
	RESULT getLength(pts_t &SWIG_OUTPUT);
	RESULT seekTo(pts_t to);
	RESULT seekRelative(int direction, pts_t to);
	RESULT getPlayPosition(pts_t &SWIG_OUTPUT);
	RESULT setTrickmode(int trick);
	RESULT isCurrentlySeekable();

		// iServiceInformation
	RESULT getName(std::string &name);
	RESULT getEvent(ePtr<eServiceEvent> &evt, int nownext);
	int getInfo(int w);
	std::string getInfoString(int w);
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	ePtr<iServiceInfoContainer> getInfoObject(int w);
#endif

		// iAudioTrackSelection
	int getNumberOfTracks();
	RESULT selectTrack(unsigned int i);
	RESULT getTrackInfo(struct iAudioTrackInfo &, unsigned int n);
	int getCurrentTrack();

		// iAudioChannelSelection
	int getCurrentChannel();
	RESULT selectChannel(int i);

		// iSubtitleOutput
	RESULT enableSubtitles(iSubtitleUser *user, SubtitleTrack &track);
	RESULT disableSubtitles();
	RESULT getSubtitleList(std::vector<SubtitleTrack> &sublist);
	RESULT getCachedSubtitle(SubtitleTrack &track);

		// iStreamedService
	RESULT streamed(ePtr<iStreamedService> &ptr);
	ePtr<iStreamBufferInfo> getBufferCharge();
	int setBufferSize(int size);

		// iAudioDelay
	int getAC3Delay();
	int getPCMDelay();
	void setAC3Delay(int);
	void setPCMDelay(int);


#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	struct audioStream
	{
		GstPad* pad;
		audiotype_t type;
		std::string language_code; /* iso-639, if available. */
		std::string codec; /* clear text codec description */
		audioStream()
			:pad(0), type(atUnknown)
		{
		}
	};
	struct subtitleStream
	{
		GstPad* pad;
		subtype_t type;
		std::string language_code; /* iso-639, if available. */
		subtitleStream()
			:pad(0)
		{
		}
	};
	struct sourceStream
	{
		audiotype_t audiotype;
		containertype_t containertype;
		bool is_audio;
		bool is_streaming;
		bool is_hls;
		sourceStream()
			:audiotype(atUnknown), containertype(ctNone), is_audio(FALSE), is_streaming(FALSE), is_hls(FALSE)
		{
		}
	};
#else  // eplayer3
	struct audioStream
	{
		audiotype_t type;
		std::string language_code; /* iso-639, if available. */
		std::string codec; /* clear text codec description */
		audioStream()
			:type(atUnknown)
		{
		}
	};
	struct subtitleStream
	{
		subtype_t type;
		std::string language_code; /* iso-639, if available. */
		int id;
		subtitleStream()
		{
		}
	};
	struct sourceStream
	{
		audiotype_t audiotype;
		containertype_t containertype;
		bool is_video;
		bool is_streaming;
		sourceStream()
			:audiotype(atUnknown), containertype(ctNone), is_video(false), is_streaming(false)
		{
		}
	};
#endif
	struct bufferInfo
	{
		gint bufferPercent;
		gint avgInRate;
		gint avgOutRate;
		gint64 bufferingLeft;
		bufferInfo()
			:bufferPercent(0), avgInRate(0), avgOutRate(0), bufferingLeft(-1)
		{
		}
	};
	struct errorInfo
	{
		std::string error_message;
		std::string missing_codec;
	};

protected:
	ePtr<eTimer> m_nownext_timer;
	ePtr<eServiceEvent> m_event_now, m_event_next;
	void updateEpgCacheNowNext();

		/* cuesheet */
	struct cueEntry
	{
		pts_t where;
		unsigned int what;

		bool operator < (const struct cueEntry &o) const
		{
			return where < o.where;
		}
		cueEntry(const pts_t &where, unsigned int what) :
			where(where), what(what)
		{
		}
	};

	std::multiset<cueEntry> m_cue_entries;
	int m_cuesheet_changed, m_cutlist_enabled;
	void loadCuesheet();
	void saveCuesheet();
private:
	static int pcm_delay;
	static int ac3_delay;
	int m_currentAudioStream;
	int m_currentSubtitleStream;
	int m_cachedSubtitleStream;
	int selectAudioStream(int i);
	std::vector<audioStream> m_audioStreams;
	std::vector<subtitleStream> m_subtitleStreams;
	iSubtitleUser *m_subtitle_widget;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	gdouble m_currentTrickRatio;
#else  // eplayer3
	int m_currentTrickRatio;
#endif
	friend class eServiceFactoryMP3;
	eServiceReference m_ref;
	int m_buffer_size;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	int m_ignore_buffering_messages;
	bool m_is_live;
	bool m_subtitles_paused;
	bool m_use_prefillbuffer;
	bool m_paused;
	bool m_first_paused;
	bool m_autoaudio;
	/* cuesheet load check */
	bool m_cuesheet_loaded;
	bool m_audiosink_not_running;
	/* servicemMP3 chapter TOC support CVR */
	bool m_use_chapter_entries;
	/* last used seek position gst-1 only */
	gint64 m_last_seek_pos;
	ePtr<eTimer> m_play_position_timer;
	void playPositionTiming();
	bool m_use_last_seek;
	bufferInfo m_bufferInfo;
	errorInfo m_errorInfo;
	std::string m_download_buffer_path;
	eServiceMP3(eServiceReference ref);
	sigc::signal2<void,iPlayableService*,int> m_event;
	enum
	{
		stIdle, stRunning, stStopped,
	};
	int m_state;
	bool m_gstdot;
#endif

#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	GstElement *m_gst_playbin, *audioSink, *videoSink;
	GstTagList *m_stream_tags;
	bool m_coverart;

	eFixedMessagePump<ePtr<GstMessageContainer> > m_pump;

	audiotype_t gstCheckAudioPad(GstStructure* structure);
	void gstBusCall(GstMessage *msg);
	void handleMessage(GstMessage *msg);
	static GstBusSyncReply gstBusSyncHandler(GstBus *bus, GstMessage *message, gpointer user_data);
	static void gstTextpadHasCAPS(GstPad *pad, GParamSpec * unused, gpointer user_data);
	void gstTextpadHasCAPS_synced(GstPad *pad);
	static void gstCBsubtitleAvail(GstElement *element, GstBuffer *buffer, gpointer user_data);
	GstPad* gstCreateSubtitleSink(eServiceMP3* _this, subtype_t type);
	void gstPoll(ePtr<GstMessageContainer> const &);
	static void playbinNotifySource(GObject *object, GParamSpec *unused, gpointer user_data);
	/* TOC processing CVR */
	void HandleTocEntry(GstMessage *msg);
	static gint match_sinktype(const GValue *velement, const gchar *type);
	static void handleElementAdded(GstBin *bin, GstElement *element, gpointer user_data);
#else  // eplayer3
	Context_t *player;

	struct Message
	{
		Message()
			:type(-1)
		{}
		Message(int type)
			:type(type)
		{}
		int type;
	};
	eFixedMessagePump<Message> m_pump;
	static void eplayerCBsubtitleAvail(long int duration_ns, size_t len, char * buffer, void* user_data);
#endif

	struct subtitle_page_t
	{
		uint32_t start_ms;
		uint32_t end_ms;
		std::string text;
		ePtr<gPixmap> pixmap;

		subtitle_page_t(uint32_t start_ms_in, uint32_t end_ms_in, const std::string& text_in)
			: start_ms(start_ms_in), end_ms(end_ms_in), text(text_in)
		{
		}
		subtitle_page_t(uint32_t start_ms_in, uint32_t end_ms_in, ePtr<gPixmap> pixmap_in)
			: start_ms(start_ms_in), end_ms(end_ms_in), pixmap(pixmap_in)
		{
		}
	};

	typedef std::map<uint32_t, subtitle_page_t> subtitle_pages_map_t;
	typedef std::pair<uint32_t, subtitle_page_t> subtitle_pages_map_pair_t;
	subtitle_pages_map_t m_subtitle_pages;
	ePtr<eTimer> m_subtitle_sync_timer;

	ePtr<eTimer> m_streamingsrc_timeout;
	pts_t m_prev_decoder_time;
	int m_decoder_time_valid_state;

	void pushSubtitles();
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	void pullSubtitle(GstBuffer *buffer);
#endif
	void sourceTimeout();
	sourceStream m_sourceinfo;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	gulong m_subs_to_pull_handler_id, m_notify_source_handler_id, m_notify_element_added_handler_id;
#endif

	RESULT seekToImpl(pts_t to);

	gint m_aspect, m_width, m_height, m_framerate, m_progressive, m_gamma;
	std::string m_useragent;
	std::string m_extra_headers;
#if defined ENABLE_GSTREAMER \
 || defined ENABLE_DUAL_MEDIAFW
	RESULT trickSeek(gdouble ratio);
#endif
	ePtr<iTSMPEGDecoder> m_decoder; // for showSinglePic when radio
};

#endif
// vim:ts=4
