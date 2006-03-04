##Captcha Readme##

To Install:
Enable the module, and then go to admin/settings/captcha to enable captchas
for various actions and captcha types.

Default, and Image Captchas:
This new version of captcha.module uses a simple math question as the
default captcha challenge. However, this is NOT as powerful as the original
image captcha, in case you are looking for foolproof protection.

The image captcha facility has been shifted to an additional submodule called
"textimage". You will need to install captcha.module AND textimage.module to
get image captchas working.
