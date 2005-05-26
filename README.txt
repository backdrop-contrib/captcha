##Captcha Readme##

To Install:
Enable the module, and then go to admin/settings/captcha to enable captchas for various actions.
Then go to admin/access control to enable the access to the captcha (access captchas) for roles.

Notes:
- Uses GD to draw stuff, emits in PNG
- Captcha for comments is contingent upon a patch(#14708) for now. One day, this will reach core.
- I've added function_exists checks for it to degrade gracefully across multiple versions of GD,
  please file issues with the exact GD, PHP information if you have problems.

Captcha Demo:
http://arnab.drupaldevs.org
