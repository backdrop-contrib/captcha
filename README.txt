Captcha Readme
--
Hi, 
Just to keep track of usage, it would be really cool if you can 
send an email to [drupal AT arnab DOT org] telling me you're using it.
-arnab
--

Notes
- Uses GD to draw stuff, emits in PNG
- Uses the _cron hook to clean up old image files - make sure you have the
  drupal cron setup, or you'll have to manually reclaim diskspace.
- Now checks for imagerotate() function that some people don't seem to have. [Dec 12 2004]

Captcha Todo - arnabdotorg
- have to check for GD.
- clean up image generation code.
- add TTF font capability.
- make captcha distortion more spiffy.

Captcha Screenshot

http://lab.arnab.org/drupal/captcha/screenshot.png
