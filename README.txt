##Captcha Readme##

To Install:
1. Enable the module, and then go to admin/settings/captcha to enable captchas for various actions.
2. Then go to admin/access control to enable the access to the captcha (access captchas) for roles.

Notes:
- Uses GD to draw stuff, emits in PNG
- I've added function_exists checks for it to degrade gracefully across multiple versions of GD,
  please file issues with the exact GD, PHP information if you have problems.