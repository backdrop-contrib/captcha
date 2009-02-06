<?php
// $Id$

/**
 * Form alter phase of form processing for untrusted users.
 */
function _captcha_form_alter_untrusted_user(&$form, $form_state, $form_id) {

  // Get CAPTCHA type and module for given form_id. Return if no CAPTCHA was set.
  $captcha_point = db_fetch_object(db_query("SELECT module, type FROM {captcha_points} WHERE form_id = '%s'", $form_id));
  if (!$captcha_point || !$captcha_point->type) {
    return;
  }

    // Prevent caching of the page with this CAPTCHA enabled form.
  // This needs to be done even if the CAPTCHA will be skipped later:
  // other untrusted users should not get a cached page when
  // the current untrusted user can skip the current CAPTCHA.
  global $conf;
  $conf['cache'] = FALSE;

  // Retrieve or generate CAPTCHA session ID.
  if (isset($form_state['post']['form_id'])
  && $form_state['post']['form_id'] == $form_id
  && isset($form_state['post']['captcha_sid'])) {
    $captcha_sid = $form_state['post']['captcha_sid'];
  } else {
    // Generate new CAPTCHA session.
    $captcha_sid = _captcha_generate_captcha_session(CAPTCHA_STATUS_UNSOLVED);
  }

  if (_captcha_required_for_user($captcha_sid, $form_id)) {

    // Generate a CAPTCHA and its solution (note that the CAPTCHA session ID
    // is given as thrid argument.
    $captcha = module_invoke($captcha_point->module, 'captcha', 'generate', $captcha_point->type, $captcha_sid);
    if (!$captcha) {
      // The selected module returned nothing, maybe it is disabled or it's wrong, we should watchdog that and then quit.
      watchdog('CAPTCHA',
        'CAPTCHA problem: hook_captcha() of module %module returned nothing when trying to retrieve challenge type %type for form %form_id.',
        array('%type' => $captcha_point->type, '%module' => $captcha_point->module, '%form_id' => $form_id),
        WATCHDOG_ERROR);
      return;
    }

    // Add a CAPTCHA part to the form (depends on value of captcha_description).
    $captcha_description = _captcha_get_description();
    if ($captcha_description) {
      // $captcha_description is not empty: CAPTCHA part is a fieldset with description.
      $form['captcha']['captcha_widgets'] = array(
        '#type' => 'fieldset',
        '#title' => t('CAPTCHA'),
        '#description' => $captcha_description,
        '#attributes' => array('class' => 'captcha'),
      );
    }
    else {
      // $captcha_description is empty: CAPTCHA part is an empty markup form element.
      $form['captcha']['captcha_widgets'] = array(
        '#type' => 'markup',
        '#prefix' => '<div class="captcha">',
        '#suffix' => '</div>',
      );
    }

    // Add the form elements of the generated CAPTCHA to the form
    $form['captcha']['captcha_widgets'] = array_merge($form['captcha']['captcha_widgets'], $captcha['form']);


    // Add pre_render callback for additional CAPTCHA processing.
    $form['#pre_render'][] = 'captcha_pre_render_untrusted_user';
    // Add pre_render callback for placement of CAPTCHA formt element (above submit buttons).
    $form['#pre_render'][] = 'captcha_pre_render_place_captcha';

    // Add a validation callback for the CAPTCHA form element.
    // It is put in front of the list of the validation callbacks (if any).
    // This is needed for user login protection, where the login is done in
    // a validation callback (user_login_authenticate_validate), so
    // captcha_validate() needs to run before that.
    $form['#validate'] = array_merge(array('captcha_validate'), (array)($form['#validate']));

  }

  $form['captcha']['captcha_sid'] = array(
    '#type' => 'hidden',
    '#value' => $captcha_sid,
  );

  // Store information for usage in the validation and pre_render phase.
  $form['captcha']['captcha_info'] = array(
    '#type' => 'value',
    '#value' => array(
      'module' => $captcha_point->module,
      'type' => $captcha_point->type,
      'captcha_sid' => $captcha_sid,
      'solution' => $captcha['solution'],
      'preprocess' => isset($captcha['preprocess'])? $captcha['preprocess'] : FALSE,
    ),
  );


}

