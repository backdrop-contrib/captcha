<?php
// $Id$

/**
 * Helper function for checking if the CAPTCHA for the given form_id should
 * be skipped because of CAPTCHA persistence.
 */
function _captcha_persistence_skip($form_id) {
  switch (variable_get('captcha_persistence', CAPTCHA_PERSISTENCE_SHOW_ALWAYS)) {
    case CAPTCHA_PERSISTENCE_SKIP_ONCE_SUCCESSFUL:
      return isset($_SESSION['captcha_success_form_ids']) && count($_SESSION['captcha_success_form_ids']);
    case CAPTCHA_PERSISTENCE_SKIP_ONCE_SUCCESSFUL_PER_FORM:
      return isset($_SESSION['captcha_success_form_ids'][$form_id]);
    default:
      return FALSE;
  }
}

/**
 * Generate the form elements for the challenge.
 *
 * @param $form_id
 *     The form_id of the form to generate the challenge for.
 * @return
 *     NULL if no CAPTCHA is associated with the form_id
 *     otherwise: a form array, with the relevant data in $form['captcha']
 */
function _captcha_generate_challenge($form_id) {
  $form = array();

  // Get CAPTCHA type and module for this form. Return if no CAPTCHA was set.
  $captcha_point = db_fetch_object(db_query("SELECT module, type FROM {captcha_points} WHERE form_id = '%s'", $form_id));
  if (!$captcha_point || !$captcha_point->type) {
    return;
  }

  // Prevent caching of the page with this CAPTCHA enabled form.
  // This needs to be done even if the CAPTCHA will be skipped (because of
  // persistence): other untrusted users should not get a cached page when
  // the current untrusted user can skip the current CAPTCHA.
  global $conf;
  $conf['cache'] = FALSE;

  // Do not present CAPTCHA if not CAPTCHA-persistent and user has already solved a CAPTCHA for this form
  if (_captcha_persistence_skip($form_id)) {
    return;
  }

  // Generate a CAPTCHA and its solution
  $captcha = module_invoke($captcha_point->module, 'captcha', 'generate', $captcha_point->type);
  if (!$captcha) {
    //The selected module returned nothing, maybe it is disabled or it's wrong, we should watchdog that and then quit.
    watchdog('CAPTCHA',
      'CAPTCHA problem: hook_captcha() of module %module returned nothing when trying to retrieve challenge type %type for form %form_id.',
      array('%type' => $captcha_point->type, '%module' => $captcha_point->module, '%form_id' => $form_id),
      WATCHDOG_ERROR);
    return;
  }

  // Add a CAPTCHA part to the form (depends on value of captcha_description)
  $captcha_description = _captcha_get_description();
  if ($captcha_description) {
    // $captcha_description is not empty: CAPTCHA part is a fieldset with description
    $form['captcha'] = array(
      '#type' => 'fieldset',
      '#title' => t('CAPTCHA'),
      '#description' => $captcha_description,
      '#attributes' => array('class' => 'captcha'),
    );
  }
  else {
    // $captcha_description is empty: CAPTCHA part is an empty markup form element
    $form['captcha'] = array(
      '#type' => 'markup',
      '#prefix' => '<div class="captcha">',
      '#suffix' => '</div>',
    );
  }

  // Add the form elements of the generated CAPTCHA to the form
  $form['captcha']['challenge'] = $captcha['form'];
  $form['captcha']['challenge']['#prefix'] = '<div id="captcha-challenge">';
  $form['captcha']['challenge']['#suffix'] = '</div>';


  // get addition challenge data if available
  if (!empty($captcha['data'])) {
    // add solution to captcha_challenges table
    db_query("INSERT into {captcha_challenges} (solution, sid, timestamp, data) VALUES ('%s', '%s', %d, '%s')", $captcha['solution'], session_id(), time(), $captcha['data']);
  }
  else {
    // add solution to captcha_challenges table
    db_query("INSERT into {captcha_challenges} (solution, sid, timestamp) VALUES ('%s', '%s', %d)", $captcha['solution'], session_id(), time());
  }


  $challenge_id = db_last_insert_id('captcha_challenges', 'challenge_id');
  $form['captcha']['challenge']['captcha_challenge_id'] = array(
    '#type' => 'hidden',
    '#value' => $challenge_id,
  );

  // other internal values needed for the validation phase
  $form['captcha']['captcha_info'] = array(
    '#type' => 'value',
    '#value' => array(
      'form_id' => $form_id,
      'preprocess' => isset($captcha['preprocess'])? $captcha['preprocess'] : FALSE,
      'module' => $captcha_point->module,
      'type' => $captcha_point->type,
    ),
  );

  if (variable_get('captcha_allow_ajax_reload', TRUE)) {
    $form['captcha']['captcha_reload'] = array(
      '#type' => 'submit',
      '#value' => t('Another challenge'),
      '#description' => t('Get another challenge'),
      '#submit' => array('captcha_reload_submit'), // If no javascript action.
      '#ahah' => array(
        'path' => 'captcha/js',
        'wrapper' => 'captcha-challenge',
        'method' => 'append',
        'effect' => 'slide',
      ),
    );
  }

  return $form;
}

/**
 * Implementation of form #validate.
 */
function captcha_validate($form, &$form_state) {

  // Get answer and preprocess if needed
  $captcha_response = $form_state['values']['captcha_response'];
  $captcha_info = $form_state['values']['captcha_info'];
  if ($captcha_info['preprocess']) {
    $captcha_response = module_invoke($captcha_info['module'], 'captcha', 'preprocess', $captcha_info['type'], $captcha_response);
  }
  $form_id = $captcha_info['form_id'];
  // Note that we use $form_state['clicked_button']['#post']['captcha_challenge_id']
  // here instead of $form_state['values']['captcha_challenge_id'], because the latter
  // contains the challenge_id of the new form, while the former contains
  // not the challenge_id of the posted form.
  $challenge_id = $form_state['clicked_button']['#post']['captcha_challenge_id'];

  $solution = db_result(db_query('SELECT solution FROM {captcha_challenges} WHERE challenge_id = %d', $challenge_id));
  if ($solution === FALSE) {
    // unknown challenge_id
    form_set_error('captcha', t('CAPTCHA test failed (unknown challenge_id).'));
  }
  else {
    // Unset the solution to prevent reuse of the same CAPTCHA solution
    $result = db_query('DELETE FROM {captcha_challenges} WHERE challenge_id = %d', $challenge_id);

    // Check answer
    if ($captcha_response == $solution) {
      $_SESSION['captcha_success_form_ids'][$form_id] = $form_id;
    }
    else {
      // set form error
      form_set_error('captcha_response', t('The answer you entered for the CAPTCHA was not correct.'));
      // update wrong response counter
      variable_set('captcha_wrong_response_counter', variable_get('captcha_wrong_response_counter', 0) + 1);
      // log to watchdog if needed
      if (variable_get('captcha_log_wrong_responses', FALSE)) {
        watchdog('CAPTCHA',
          '%form_id post blocked by CAPTCHA module: challenge "%challenge" (by module "%module"), user answered "%response", but the solution was "%solution".',
          array('%form_id' => $form_id,
            '%response' => $captcha_response, '%solution' => $solution,
            '%challenge' => $captcha_info['type'], '%module' => $captcha_info['module'],
          ),
          WATCHDOG_NOTICE);
      }
      // If CAPTCHA was on a login form: stop validating, quit the current request
      // and forward to the current page (like a reload) to prevent loging in.
      // We do that because the log in procedure, which happens after
      // captcha_validate(), does not check error conditions of extra form
      // elements like the CAPTCHA.
      if ($form_id == 'user_login' || $form_id == 'user_login_block') {
        drupal_goto($_GET['q']);
      }
    }
  }
}

/**
 * Implementation of form #pre_render.
 */
function captcha_pre_render($form) {
  $form_id = $form['captcha']['captcha_info']['#value']['form_id'];
  // Unset the CAPTCHA if non-CAPTCHA persistent and the CAPTCHA has
  // already been successfully solved for this form.
  // This needs to be done in this pre_render phase when previewing for example
  // nodes and comments before submission.
  // On submission of such a forms for preview, captcha_form_alter() is called
  // *before* the CAPTCHA validation function (which sets
  // $_SESSION['captcha'][$form_id]['success'] to TRUE on a correctly answered
  // CAPTCHA). After this the form_values are entered in the generated form
  // and this form is presented with the preview.
  // This means that captcha_form_alter() can't know if the CAPTCHA was
  // correctly answered and consequently adds a CAPTCHA to the form.
  // The pre_render phase happens after the validation phase and makes it
  // possible to remove the CAPTCHA from the form after all.
  if (_captcha_persistence_skip($form_id)) {
    unset($form['captcha']);
    return $form;
  }

  // empty the value of the captcha_response form item before rendering
  $form['captcha']['captcha_response']['#value'] = '';
  return $form;
}

/**
 * Pre_render function to place the CAPTCHA form element just above the last submit button
 */
function captcha_pre_render_place_captcha($form) {
  // search the weights of the buttons in the form
  $button_weights = array();
  foreach (element_children($form) as $key) {
    if ($key == 'buttons' || isset($form[$key]['#type']) && ($form[$key]['#type'] == 'submit' || $form[$key]['#type'] == 'button')) {
      $button_weights[] = $form[$key]['#weight'];
    }
  }
  if ($button_weights) {
    // set the weight of the CAPTCHA element a tiny bit smaller than the lightest button weight
    // (note that the default resolution of #weight values is 1/1000 (see drupal/includes/form.inc))
    $first_button_weight = min($button_weights);
    $form['captcha']['#weight'] = $first_button_weight - 0.5/1000.0;
    // make sure the form gets sorted before rendering
    unset($form['#sorted']);
  }
  return $form;
}

/**
 * AJAX callback for challenge reload
 */
function captcha_js() {
  //TODO: update cached form (form is automatically cached when ahah is set?)
  $form_id = $_POST['form_id'];
  $_POST['captcha_challenge_id']; //TODO: remove this unused challenge?

  // Generate challenge for the form (if any) and add it to the form
  $captcha_elements = _captcha_generate_challenge($form_id);
  if (!$captcha_elements) {
    // no CAPTCHA needed for this form_id
    drupal_json(array('status' => FALSE, 'data' => ''));
    exit();
  }
  // build and render
  $form = $captcha_elements['captcha'];
  $form += array(
    '#post' => $_POST,
    '#programmed' => FALSE,
    '#tree' => FALSE,
    '#parents' => array(),
  );
  $form_state = array('submitted' => FALSE);
  $form = form_builder('captcha_js', $form, $form_state);
  $output = theme('status_messages') . drupal_render($form['challenge']);

  drupal_json(array('status' => TRUE, 'data' => $output));
  exit();
}
