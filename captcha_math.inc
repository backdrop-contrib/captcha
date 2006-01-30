<?

function _captcha_math_challenge(&$form, &$captcha) {

  $x = rand(1,10);
  $y = rand(1,10);

  $captcha = ($x + $y) . '';

  $form['captcha_response'] = array (
    '#type' => 'textfield',
    '#title' => t('Math Question: What is '. $x .' + '. $y .'?'),
    '#defaultvalue' => '',
    '#description' => t('Please solve the math problem above and type in the result. e.g. for 1+1, type 2'),
    '#weight' => 0,
    '#required' => TRUE,
    '#validate' => array('_captcha_validate' => array())
  );

}

function _captcha_math_validate(&$captcha_word, &$correct) {
  $captcha_word = drupal_strtolower($captcha_word);
  if ($captcha_word == $_SESSION['captcha']) {
    $correct = TRUE;
  }
  else {
    $correct = FALSE;
    form_set_error('captcha_response', t('The answer you entered to the math problem is incorrect.'));
  }
}

?>
