/**
 * @file trill.h
 * @brief Main header file for the Trillbit SDK.
 * 
 * @author Ravikiran Bukkasagara
 * @date 10-Mar-2022
 * 
 */

#ifndef INCLUDE_TRILL_H_
#define INCLUDE_TRILL_H_

/**
 * @brief SDK Version
 * 
 */
#define TRILL_SDK_VERSION "3.2"

/**
 * @brief Maximum number of RX channels to allow.
 * 
 */
#define TRILL_MAX_RX_CHANNELS	4

/**
 * @brief Number of audio blocks to allocate for decoder path.
 * Total size of audio buffer will block_size * number of audio blocks.
 * 
 */
#define TRILL_CONFIG_NUM_RX_AUDIO_BLOCKS 10

/**
 * @brief Number of audio blocks to allocate for modulation path.
 * Total size of audio buffer will block_size * number of audio blocks.
 */
#define TRILL_CONFIG_NUM_TX_AUDIO_BLOCKS 2

/**
 * @brief After a packet a transmitted these many sample points 
 * will be sent as with zero value.
 * 
 */
#define TRILL_CONFIG_TX_FLUSH_ZERO_POINTS 1024

/**
 * @brief Either FFT or DOT product based method can be used for symbol
 * decoding. 
 * DOT product method with pregenerated sine/cosine table consumes more memory but
 * is faster and accurate.
 * DOT product method without pregenerated table consumes less memory but is slower.
 * 
 * FFT method consumes less memory, its faster but less accurate.
 * 
 * For actual metrics refer to documentation.
 */
enum
{
	TRILL_CONFIG_DATA_DECODE_METHOD_DOT_PRODUCT = 0,
	TRILL_CONFIG_DATA_DECODE_METHOD_FFT
};

/**
 * @brief Enable logging of internal state. 
 * Currently log levels are not supported. 
 * Application needs to supply logger function in init options 
 * to display log messages.
 */
#define TRILL_CONFIG_EN_LOGGING	0

/**
 * @brief Enable logging of performance metrics.
 * The logger function supplied should be fast enough else PHY/Data Link layer
 * performance will be affected.
 */
#define TRILL_CONFIG_EN_PERFORMANCE_LOG	0
#define TRILL_CONFIG_EN_CTS_PERFORMANCE_LOG 0
#define TRILL_CONFIG_EN_AUDIO_BUF_OVERFLOW_LOGGING	0

/**
 * @brief Maximum RX/TX payload size.
 * Longer the payload slower the communication for given sample rate.
 * 
 */
#define TRILL_MAX_DATA_PAYLOAD_LEN 	256

/**
 * @brief Maximum buffer size for License data.
 * 
 */
#define TRILL_MAX_LICENSE_STRING_SIZE	4096

/**
 * @brief Notification of events from audio buffer.
 * Not meant for customers. Only for internal debugging.
 */
typedef enum {
	TRILL_AUDIO_BUFFER_NOTIFY_WRITE, 
	TRILL_AUDIO_BUFFER_NOTIFY_READ
} trill_audio_buf_notify_ids_t;

/**
 * @brief Transmit Data configuration options based on Range/Distance.
 * 
 */
typedef enum
{
	TRILL_DATA_CFG_RANGE_AUTO,
	TRILL_DATA_CFG_RANGE_NEAR,
	TRILL_DATA_CFG_RANGE_MID,
	TRILL_DATA_CFG_RANGE_FAR,
	// add new configurations here.

	// Last configuration
	TRILL_DATA_CFG_UNKNOWN,
} trill_data_config_range_t;

/**
 * @brief Data Link Layer Events
 * 
 */
typedef enum
{
	TRILL_DATA_LINK_EVT_ID_CHECK_REQ, /**< ID check packet received. Used 
										by peer to get information about this device.
										*/
	TRILL_DATA_LINK_EVT_DATA_RCVD,   /**< Data packet received. */
	TRILL_DATA_LINK_EVT_DATA_SENT,   /**< Data packet sent. Read to send next packet. */
} trill_data_link_event_t;

/**
 * @brief Data Link layer supported encryption schemes
 * 
 */
typedef enum
{
	SSI_PLAIN_TEXT=0, /**< No encryption. Payload is transferred as plain text. */
	SSI_SIMPLE,			/**< Subset of RFC8439. Reduced NONCE and TAG size.*/
	SSI_FULL_RFC8439=6, /**< Full compliant to RFC8439. */
	SSI_ASYMMETRIC 		/**< Asymmetric key exchange. NOT yet implemented */
} trill_data_encryption_scheme_t;

/**
 * @brief Trill Process return values indications.
 * 
 */
enum 
{
	TRILL_PROC_CTS_SEARCH,
	TRILL_PROC_DEMOD_PROGRESS,
	TRILL_PROC_MOD_PACKET_SENT,
};

/**
 * @brief Data Link layer callback parameter.
 * 
 */
typedef struct {
	trill_data_link_event_t event; /**< Event code. Refer to trill_data_link_event_t */
	unsigned char* payload;	/**< Payload data. Pointer should not be used after returning from this callback. */
	unsigned int payload_len; /**< Length of the payload in bytes. */
	int ssi;	/**< Security scheme that was used for this packet. */
	trill_data_config_range_t data_cfg_range; /**< Range Config of received data. */
	void* user_data; /**< User provided data. */
	int channel;
} trill_data_link_event_params_t;

typedef void (*trill_audio_buf_notify_cb_t)(trill_audio_buf_notify_ids_t event);
typedef void (*trill_data_link_cb_t)(const trill_data_link_event_params_t* params);

/**
 * @brief Called by SDK to enable/disable audio transmitter path.
 * When audio transmitter is enabled, receiver should be disabled.
 * @param enable When 1, enable Tx path and disable Rx path. 
 * 				When 0, disable Tx path and enable Rx path.
 *
 */
typedef void (*trill_audio_tx_enable_cb_t)(int enable);

typedef void* (*mem_alloc_fn_t) (unsigned int size);
typedef void* (*mem_aligned_alloc_fn_t) (unsigned int alignment, unsigned int size);
typedef void (*mem_free_fn_t) (void*);
typedef void (*print_fn_t)(const char*);
typedef unsigned long (*timer_get_fn_t)(void);

/**
 * @brief Fill dest buffer with size random bytes.
 * Should return 1 on success else 0.
 * 
 */
typedef int (*trng_fn_t)(unsigned char* dest, unsigned size);

/**
 * @brief Initialization options for the SDK.
 * 
 */
typedef struct {
	unsigned char n_rx_channels; /**<Total number of RX Channels/Slots present in input block.*/
	unsigned int rx_channels_en_bm; /**< (Bitmap) Which rx channel/slot to process in input block.
		NOTE: On some systems actual number of MICs can be less than 
		number of channels. 
		Examples:
		1) Single mic but stereo output.
		2) TDM slots with mix of MICs and ADC data.
		Set the corresponding bit to enable processing of that channel.
		Bit 0 = Channel 0, Bit 1 = Channel 1, ... 
		Block size for "trill_add_audio_block" call should be:
		n_rx_channels * samples_per_block * sample_size 
		Block added is assumed to be interleaved as follows:
		+-----------+-----------+-----+-------------+-----------+-----+
		| Channel 0 | Channel 1 | ... | Channel N-1 | Channel 0 | ... |
		+-----------+-----------+-----+-------------+-----------+-----+ 
		*/


	unsigned int aud_buf_rx_block_size_bytes; /**< Size of Rx/detector audio buffer block.
				Each sample size in a block is assumed to be 16 bits (Q15).
				Block size should be = samples_per_block * sample_size
				*/
	unsigned int aud_buf_rx_n_blocks; /**< Number of Rx audio buffer blocks. */
	trill_audio_buf_notify_cb_t aud_buf_rx_notify_cb; /**< Rx audio buffer notification 
												callback. Can be NULL.
												*/

	unsigned int aud_buf_tx_block_size_bytes; /**< Size of Tx audio buffer block.
				Each sample size in a block is 16 bits (Q15).
				 */
	unsigned int aud_buf_tx_n_blocks; /**< Number of Tx audio buffer blocks. */
	trill_audio_buf_notify_cb_t aud_buf_tx_notify_cb; /**< Set to NULL. Reserved for future. */
	trill_audio_tx_enable_cb_t audio_tx_enable_fn; /**< Called by SDK to 
					enable audio transmitter. */

	int aud_buf_en_rx_add_block; /**< Set it to non-zero value to enable blocking 
				if audio buffer is full while adding blocks. */

	trill_data_link_cb_t data_link_cb; /**< Called by SDK to notify data 
				link layer events. */
	void* data_link_cb_user_data; /**< User data. Will be returned in call back. */
	
	const char* b64_license; /**< Base64 encoded and null terminated 
				license data as received from server. */
	
	const char* b64_ck; /**< Base64 encoded and null terminated 
				Communication Key as received from server. */

	const char* b64_ck_nonce; /**< Base64 encoded and null terminated 
				Communication Key Nonce if received from server. Can be NULL. */

	mem_alloc_fn_t mem_alloc_fn; /**< Platform specific memory allocation function. 
				SDK will use it to allocate memory. */
	
	mem_aligned_alloc_fn_t mem_aligned_alloc_fn; /**< Platform specific aligned memory allocation function. 
				SDK will use it to allocate memory. */
	
	mem_free_fn_t mem_free_fn; /**< Platform specific memory de-allocation function. 
				SDK will use it to free memory. */

	print_fn_t logger_fn; /**< SDK will use this to log formatted string messages. */

	timer_get_fn_t timer_get_fn; /**< SDK will use this function for 
				performance measurements.
				Timer precision is assumed to be 1 usec. */
	trng_fn_t trng_fn; /**< Can be NULL if TRNG feature is not available, 
					however this will weaken security.*/
} trill_init_opts_t;

/**
 * @brief Tx packet security parameters.
 * 
 */
typedef struct {
	trill_data_encryption_scheme_t ssi; /**< Security scheme to use for tx packet. */
	unsigned char* ck_nonce;	/**< Override the nonce to be used for tx encryption. */
	trill_data_config_range_t data_cfg_range; /**< Range Config to use to send data symbols. */
} trill_tx_params_t;

/**
 * @brief Initialze the SDK.
 * First function to be called before SDK can be used.
 *  
 * @param init_opt Initialization options. Refer to trill_init_opts_t
 * @param handle SDK Handle. Preserve it to pass with other calls.
 * @return int  0 on success else negative error code.
 */
int trill_init(const trill_init_opts_t* init_opt, void** handle);

/**
 * @brief Rx path processing function.
 * Call from a separate task/thread in a loop. Refer to example code.
 * 
 * @return int Processing state: 
 *	TRILL_PROC_CTS_SEARCH 		= CTS Hunting mode.
 *	TRILL_PROC_DEMOD_PROGRESS  	= Demodulation in progress.
 *  TRILL_PROC_MOD_PACKET_SENT 	= Modulated packet was sent.
 *	Negative error code.
 */
int trill_process(void* handle);

/**
 * @brief Tx path processing function.
 * Call from the same task/thread which is calling trill_process_input.
 * Refer to example code.
 * 
 * @return int Processing state:
 *	0 = Packet was successfully sent.
 *	1 = IDLE, call again. 
 *	Negative error code.
 */
//int trill_process_output(void* handle);

/**
 * @brief Call when next Rx audio block is available.
 * block size should be same as given in the SDK initialization options.
 * 
 * This function can be called from ISR.
 *  
 * @param block Block data address. Block can be reused after call returns.
 * @return int 0 on success else negative error code.
 */
int trill_add_audio_block(void* handle, const void* block);

/**
 * @brief Call when next Tx audio block can be sent.
 * This function will not block even when no Tx block is available from audio buffer,
 * when this scenario occurs it will lead to Tx underrun.
 * 
 * This function can be called from ISR.
 * 
 * The size of the acquired block is same as given in 
 * the SDK initialization options.
 * 
 * @param addr Address of the block inside the audio buffer. 
 * 	Caller should copy the data and call trill_release_audio_block.
 * @param en_blocking If non-zero, call will block until audio block is available.
 * Set to zero if calling from ISR. Set to non-zero if calling from task/thread.
 * @return int 0 on success else TRILL_ERR_AUDIO_TX_BLOCK_NOT_AVAILABLE when
 *	no Tx block is available.
 */
int trill_acquire_audio_block(void* handle, short** addr, int en_blocking);

/**
 * @brief Call after successfully acquiring a Tx audio block to return it to
 * Tx audio buffer.
 * 
 * @return int 
 */
int trill_release_audio_block(void* handle);

/**
 * @brief Send Tx Packet. 
 * Call it from a task/thread other than the one being used 
 * to call trill_process_input / trill_process_output functions.
 * 
 * When this function returns success, packet is prepared but not yet sent.
 * When sent successfully data link layer will send notification callback.
 * 
 * @param security_params Refer to trill_tx_params_t
 * @param data Payload data to be sent. data pointer can be reused 
 * after the function returns. 
 * @param data_len Payload size in bytes.
 * @return int 0 on packet accepted to be sent else negative error code.
 */
int trill_tx_data(void* handle, trill_tx_params_t* security_params, 
	unsigned char* data, unsigned int data_len);

/**
 * @brief Abort Tx data if in progress or about to be sent.
 * Even after successful abort there might be unread audio blocks in audio buffer.
 * These needs to be flushed out by application.
 * 
 * @param handle 
 * @return int 0 on success else negative error code.
 */
int trill_tx_abort(void* handle);

/**
 * @brief Retrieve the device ID of this MCU as represented by SDK internally.
 * 
 * Use the retrieved device id for data exchange with server APIs.
 * 
 * @param buf Address of the buffer to receive null terminated device id string.
 * @return int 0 on success else negative error code.
 */
int trill_get_device_id(const char** buf);

/**
 * @brief Can be used to verify whether device License is valid.
 * 
 * @return int Positive number if License is valid else 0.
 */
int trill_is_device_verified(void* handle);

/**
 * @brief DeInitialize the SDK and release all memories.
 * 
 * @param handle 
 * @return int error code
 */
int trill_deinit(void* handle);

/**
 * @brief Generate combiniation of given Sine tones to test audio tx path.
 * Useful for testing on a new board before using the sdk.
 * This signal will be played on tx side just like sending regular packet.
 * 
 * @param handle Trill handle received from trill init.
 * @param freq Array of sine tone frequencies. 
 * @param n_freq Number of frequencies in the array. Max 10 frequencies allowed.
 * Each should be less than 20KHz, Sampling rate fixed at 48KHz. Amplitude fixed at 1.0.
 * @param n_points Number of points to generate and send.
 * 
 * @return 0 on success else negative error code.
 */
int trill_tx_tones(void* handle, unsigned int* freq, unsigned int n_freq, unsigned int n_points);

/**
 * @brief  Report the last signal frequency seen when idle.
 * Useful to test mic-to-rx path on a new board. Play external sine tone and 
 * call this function to check the frequncy seen by internal algorithm.
 * 
 * @param handle Trill handle
 * @param freq Pointer to save last frequency.
 * @param mag Pointer to save magnitude of last frequency.
 * @return int
 */
int trill_rx_get_last_freq(void* handle, unsigned int* freq, float* mag);

#endif /* INCLUDE_TRILL_H_ */
