/*
 * Jamvox USB Audio Interface Driver for Linux
 * Compatible with Ubuntu 22.04 and kernel 5.15+
 * 
 * This driver supports the VOX Jamvox USB audio interface
 * Vendor ID: 0x0944, Product ID: 0x0117
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#define DRIVER_NAME "jamvox"
#define VENDOR_ID 0x0944
#define PRODUCT_ID 0x0117
#define JAMVOX_MAX_CHANNELS 2
#define JAMVOX_SAMPLE_RATE 44100
#define JAMVOX_BUFFER_SIZE 4096

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jamvox Driver Team");
MODULE_DESCRIPTION("VOX Jamvox USB Audio Interface Driver");
MODULE_VERSION("1.0.0");

struct jamvox_device {
    struct usb_device *udev;
    struct snd_card *card;
    struct snd_pcm *pcm;
    struct usb_interface *intf;
    struct snd_pcm_substream *playback_substream;
    struct snd_pcm_substream *capture_substream;
    struct urb *playback_urb;
    struct urb *capture_urb;
    u8 *playback_buffer;
    u8 *capture_buffer;
    dma_addr_t playback_dma;
    dma_addr_t capture_dma;
    int playback_running;
    int capture_running;
};

static struct snd_pcm_hardware jamvox_pcm_hw = {
    .info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
             SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
    .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE,
    .rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
    .rate_min = 44100,
    .rate_max = 48000,
    .channels_min = 1,
    .channels_max = JAMVOX_MAX_CHANNELS,
    .buffer_bytes_max = JAMVOX_BUFFER_SIZE * 4,
    .period_bytes_min = 64,
    .period_bytes_max = JAMVOX_BUFFER_SIZE,
    .periods_min = 2,
    .periods_max = 32,
};

static int jamvox_pcm_open(struct snd_pcm_substream *substream) {
    struct jamvox_device *dev = snd_pcm_substream_chip(substream);
    substream->runtime->hw = jamvox_pcm_hw;
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        dev->playback_substream = substream;
    else
        dev->capture_substream = substream;
    return 0;
}

static int jamvox_pcm_close(struct snd_pcm_substream *substream) {
    struct jamvox_device *dev = snd_pcm_substream_chip(substream);
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        dev->playback_substream = NULL;
    else
        dev->capture_substream = NULL;
    return 0;
}

static int jamvox_pcm_hw_params(struct snd_pcm_substream *substream,
                                struct snd_pcm_hw_params *hw_params) { return 0; }
static int jamvox_pcm_hw_free(struct snd_pcm_substream *substream) { return 0; }
static int jamvox_pcm_prepare(struct snd_pcm_substream *substream) { return 0; }

static void jamvox_playback_complete(struct urb *urb) {
    struct jamvox_device *dev = urb->context;
    if (!dev->playback_running) return;
    if (urb->status == 0) {
        if (dev->playback_substream)
            snd_pcm_period_elapsed(dev->playback_substream);
        usb_submit_urb(urb, GFP_ATOMIC);
    }
}

static void jamvox_capture_complete(struct urb *urb) {
    struct jamvox_device *dev = urb->context;
    if (!dev->capture_running) return;
    if (urb->status == 0) {
        if (dev->capture_substream)
            snd_pcm_period_elapsed(dev->capture_substream);
        usb_submit_urb(urb, GFP_ATOMIC);
    }
}

static int jamvox_pcm_trigger(struct snd_pcm_substream *substream, int cmd) {
    struct jamvox_device *dev = snd_pcm_substream_chip(substream);
    int is_playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
        if (is_playback) {
            dev->playback_running = 1;
            return usb_submit_urb(dev->playback_urb, GFP_ATOMIC);
        } else {
            dev->capture_running = 1;
            return usb_submit_urb(dev->capture_urb, GFP_ATOMIC);
        }
    case SNDRV_PCM_TRIGGER_STOP:
        if (is_playback) {
            dev->playback_running = 0;
            usb_kill_urb(dev->playback_urb);
        } else {
            dev->capture_running = 0;
            usb_kill_urb(dev->capture_urb);
        }
        return 0;
    }
    return -EINVAL;
}

static snd_pcm_uframes_t jamvox_pcm_pointer(struct snd_pcm_substream *substream) {
    return 0;
}

static struct snd_pcm_ops jamvox_pcm_ops = {
    .open = jamvox_pcm_open,
    .close = jamvox_pcm_close,
    .hw_params = jamvox_pcm_hw_params,
    .hw_free = jamvox_pcm_hw_free,
    .prepare = jamvox_pcm_prepare,
    .trigger = jamvox_pcm_trigger,
    .pointer = jamvox_pcm_pointer,
};

static int jamvox_init_audio(struct jamvox_device *dev) {
    int ret = snd_pcm_new(dev->card, DRIVER_NAME, 0, 1, 1, &dev->pcm);
    if (ret < 0) return ret;
    dev->pcm->private_data = dev;
    strcpy(dev->pcm->name, "Jamvox");
    snd_pcm_set_ops(dev->pcm, SNDRV_PCM_STREAM_PLAYBACK, &jamvox_pcm_ops);
    snd_pcm_set_ops(dev->pcm, SNDRV_PCM_STREAM_CAPTURE, &jamvox_pcm_ops);
    snd_pcm_set_managed_buffer_all(dev->pcm, SNDRV_DMA_TYPE_CONTINUOUS,
                                    NULL, JAMVOX_BUFFER_SIZE * 4, JAMVOX_BUFFER_SIZE * 4);
    dev->playback_urb = usb_alloc_urb(0, GFP_KERNEL);
    dev->capture_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!dev->playback_urb || !dev->capture_urb) return -ENOMEM;
    dev->playback_buffer = usb_alloc_coherent(dev->udev, JAMVOX_BUFFER_SIZE,
                                              GFP_KERNEL, &dev->playback_dma);
    dev->capture_buffer = usb_alloc_coherent(dev->udev, JAMVOX_BUFFER_SIZE,
                                             GFP_KERNEL, &dev->capture_dma);
    if (!dev->playback_buffer || !dev->capture_buffer) return -ENOMEM;
    usb_fill_bulk_urb(dev->playback_urb, dev->udev, usb_sndbulkpipe(dev->udev, 1),
                      dev->playback_buffer, JAMVOX_BUFFER_SIZE, jamvox_playback_complete, dev);
    usb_fill_bulk_urb(dev->capture_urb, dev->udev, usb_rcvbulkpipe(dev->udev, 1),
                      dev->capture_buffer, JAMVOX_BUFFER_SIZE, jamvox_capture_complete, dev);
    dev->playback_urb->transfer_dma = dev->playback_dma;
    dev->capture_urb->transfer_dma = dev->capture_dma;
    dev->playback_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    dev->capture_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    return 0;
}

static int jamvox_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(intf);
    struct jamvox_device *dev;
    int ret;
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;
    dev->udev = udev;
    dev->intf = intf;
    ret = snd_card_new(&intf->dev, -1, NULL, THIS_MODULE, 0, &dev->card);
    if (ret < 0) { kfree(dev); return ret; }
    dev->card->private_data = dev;
    strcpy(dev->card->driver, DRIVER_NAME);
    strcpy(dev->card->shortname, "Jamvox");
    snprintf(dev->card->longname, sizeof(dev->card->longname),
             "VOX Jamvox at %s", dev_name(&udev->dev));
    ret = jamvox_init_audio(dev);
    if (ret < 0) { snd_card_free(dev->card); kfree(dev); return ret; }
    ret = snd_card_register(dev->card);
    if (ret < 0) { snd_card_free(dev->card); kfree(dev); return ret; }
    usb_set_intfdata(intf, dev);
    return 0;
}

static void jamvox_disconnect(struct usb_interface *intf) {
    struct jamvox_device *dev = usb_get_intfdata(intf);
    if (!dev) return;
    if (dev->playback_urb) { usb_kill_urb(dev->playback_urb); usb_free_urb(dev->playback_urb); }
    if (dev->capture_urb) { usb_kill_urb(dev->capture_urb); usb_free_urb(dev->capture_urb); }
    if (dev->playback_buffer)
        usb_free_coherent(dev->udev, JAMVOX_BUFFER_SIZE, dev->playback_buffer, dev->playback_dma);
    if (dev->capture_buffer)
        usb_free_coherent(dev->udev, JAMVOX_BUFFER_SIZE, dev->capture_buffer, dev->capture_dma);
    snd_card_free(dev->card);
    kfree(dev);
}

static struct usb_device_id jamvox_id_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, jamvox_id_table);

static struct usb_driver jamvox_driver = {
    .name = DRIVER_NAME,
    .probe = jamvox_probe,
    .disconnect = jamvox_disconnect,
    .id_table = jamvox_id_table,
};

module_usb_driver(jamvox_driver);
