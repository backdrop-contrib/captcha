<?

function _captcha_image_challenge(&$form) {

  $form['captcha_image'] = array (
    '#type' => 'item',
    '#title' =>  'captcha image',
    '#weight' => 0,
    '#value' => '<img src="' . url('captcha/image/'.time()) . '"  alt="Captcha Image: you will need to recognize the text in it."/>',
  );

  $form['captcha_response'] = array (
    '#type' => 'textfield',
    '#title' => t('Word'),
    '#defaultvalue' => '',
    '#weight' => 0,
    '#required' => TRUE,
    '#validate' => array('_captcha_validate' => array()),
    '#description' => t('Please type in the letters/numbers that are shown in the image above.')
  );

  return $form;
}

function _captcha_image_validate(&$captcha_word, &$correct) {

  $captcha_word = drupal_strtolower($captcha_word);
  if ($captcha_word == $_SESSION['captcha']) {
    $correct = true;
  }
  else {
    $correct = false;
    form_set_error('captcha_response', t('The image verification code you entered is incorrect.'));
  }

}
/**
* Prints an image containing a captcha code.
*/
function _captcha_image() {

    // there are a few hard coded functions I'd like to eliminate here,
    // but for the time being we'll let them be.

    //if we don't have GD2 functions, we can't generate the image
    if (!function_exists('imagecreatetruecolor')) return;

    // Set headers
    header('Expires: Mon, 01 Jan 1997 05:00:00 GMT');
    header('Cache-Control: no-store, no-cache, must-revalidate');
    header('Cache-Control: post-check=0, pre-check=0', false);
    header('Pragma: no-cache');

    header('Content-type: image/png');

    $string = _captcha_code();

    // set up image, the first number is the width and the second is the height
    $im = imagecreatetruecolor(180, 80);

    // creates two variables to store color
    $background = imagecolorallocate($im, rand(180, 250), rand(180, 250), rand(180, 250));
    $foreground = imagecolorallocate($im, rand(0, 80), rand(0, 80), rand(0, 80));

    // fill image with bgcolor
    imagefill($im, 0, 0, $background);

    // Get truetype font list
    $fonts = _captcha_font_list();

    // writes string
    if (function_exists(imagettftext) && count($fonts) > 0) {

      // write text using a truetype font
      $charSize = 24;        // font size
      $charWidth = 0;        // width of previous character
      $x = 10;               // initial x position
      $y = 30;

      // iterate over characters
      for ($i=0;$i<drupal_strlen($string);$i++) {
        // define angle and position of character based on previous character dimension
        $x += ($charWidth * rand(1.0, 1.6));
        $y += rand(-5,5);
        $charAngle = rand(-5,5);
        $charSize += rand(-2,2);
        $char = drupal_substr($string,$i,1);

        // select random font
        $font = $fonts[rand(0,count($fonts)-1)];

        // draw character
        imagettftext($im,$charSize,$charAngle,$x,$y,$foreground,$font,$char);

        // capture character dimensions to increment x position
        $bbox = imagettfbbox($charSize,$charAngle,$font,$char);
        $charWidth = max($bbox[0],$bbox[2],$bbox[4],$bbox[6]) - min($bbox[0],$bbox[2],$bbox[4],$bbox[6]);
      }
    }

    else {
      // write text using a built-in font
      $x = 10;
      $y = 0;

      for ($i=0;$i<drupal_strlen($string);$i++) {
        imagestring($im,5,$x,$y,drupal_substr($string,$i,1),$foreground);
        $x += rand(10,15);
        $y += rand(-4,4);
      }

    }

    // strikethrough
    imageline($im, rand(0, 120), rand(0, 120), rand(0, 120), rand(0, 120), $foreground);

    // rotate only if function is defined (many PHP installations have this function missing)
    if (function_exists('imagerotate')) {
      $im2 = imagerotate($im,rand(-20,45),$background);
      imagedestroy($im);
      $im = $im2;
    }

    // add cloud only if function is defined (many PHP installations have this function missing)
    if (function_exists('imagecolorallocatealpha')) {
      $middleground = imagecolorallocatealpha($im, rand(160, 200), rand(160, 200), rand(160, 200), 80);

      // random shapes
      for ($x=0; $x<50;$x++) {
        imageline($im, rand(0, 120), rand(0, 120), rand(0, 120), rand(0, 120), $middleground);
        imageellipse($im, rand(0, 120), rand(0, 120), rand(0, 120), rand(0, 120), $middleground);
      }
    }

    //output to browser
    imagepng($im);
    imagedestroy($im);

}

/**
* Returns a random string for use in a captcha
*/
function _captcha_code() {

    $consts='bcdgjxvmnprst';
    $vowels='aeiou';

    for ($x=0; $x < 6; $x++) {
      mt_srand ((double) microtime() * 1000000);
      $const[$x] = drupal_substr($consts,mt_rand(0,drupal_strlen($consts)-1),1);
      $vow[$x] = drupal_substr($vowels,mt_rand(0,drupal_strlen($vowels)-1),1);
    }

    $string = $const[0] . $vow[0] .$const[2] . $const[1] . $vow[1] . $const[3] . $vow[3] . $const[4];
    $string = drupal_substr($string,0,rand(5,8));

    //everytime we create a new code, we write it to session
    $_SESSION['captcha'] = drupal_strtolower($string);

    return $string;

}

?>
