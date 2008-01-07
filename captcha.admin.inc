<?php
// $Id$

/**
 * Return an array with the available CAPTCHA types, for use as options array
 * for a select form elements.
 * The array is an associative array mapping "$module/$type" to
 * "$type ($module)" with $module the module name implementing the CAPTCHA
 * and $type the name of the CAPTCHA type.
 * (It also includes a 'none' => '<none>' option)
 */
function _captcha_available_challenge_types() {
  $captcha_types['none'] = '<'. t('none') .'>';
  foreach (module_implements('captcha') as $module) {
    $result = call_user_func_array($module .'_captcha', 'list');
    if (is_array($result)) {
      foreach ($result as $type) {
        $captcha_types["$module/$type"] = "$type ($module)";
      }
    }
  }
  return $captcha_types;
}

/**
 * General CAPTCHA settings handler. Main entry point for CAPTCHA management.
 *
 * If arguments are given: first argument is used as form_id, the second one
 * is interpreted as action (such as disable, delete and enable) to execute on
 * the form_id.
 * Otherwise: returns the general CAPTCHA configuration form.
 */
function captcha_admin($form_id = '', $op = '') {
  // if $form_id and action $op given: do the action
  if ($form_id) {
    switch ($op) {
      case 'disable':
        // disable the CAPTCHA for the form: set the module and type to NULL
        db_query("UPDATE {captcha_points} SET module = NULL, type = NULL WHERE form_id = '%s'", $form_id);
        drupal_set_message(t('Disabled CAPTCHA for form %form_id.', array('%form_id' => $form_id)));
        // goto the CAPTCHA administration or alternative destination if present in URI
        drupal_goto('admin/user/captcha');
        break;
      case 'delete':
        db_query("DELETE FROM {captcha_points} WHERE form_id = '%s'", $form_id);
        drupal_set_message(t('Deleted CAPTCHA for form %form_id.', array('%form_id' => $form_id)));
        // goto the CAPTCHA administration or alternative destination if present in URI
        drupal_goto('admin/user/captcha');
        break;
      case 'enable':
        db_query("DELETE FROM {captcha_points} WHERE form_id = '%s'", $form_id);
        db_query("INSERT INTO {captcha_points} (form_id, module, type) VALUES ('%s', NULL, NULL)", $form_id);
        // No drupal_goto() call because we have to go to the CAPTCHA administration
        // form and not a different destination if that would be present in the
        // URI. So we call this form explicitly. The destination will be preserved
        // so after completing the form, the user will still be redirected.
        return drupal_get_form('captcha_captcha_point_settings_form', $form_id);
        break;
    }
    // return edit form for individual form (aka CAPTCHA point)
    return drupal_get_form('captcha_captcha_point_settings_form', $form_id);
  }
  // no $form_id or legal action given: generate general CAPTCHA settings form
  return drupal_get_form('captcha_admin_settings');
}

/**
 * Form builder function for the general CAPTCHA configuration
 */
function captcha_admin_settings() {
  // field for the CAPTCHA administration mode
  $form['captcha_administration_mode'] = array(
    '#type' => 'checkbox',
    '#title' => t('Add CAPTCHA administration links to forms'),
    '#default_value' => variable_get('captcha_administration_mode', FALSE),
    '#description' => t('This option is very helpful to enable/disable challenges on forms. When enabled, users with the "%admincaptcha" permission will see CAPTCHA administration links on all forms (except on administrative pages, which shouldn\'t be accessible to untrusted users in the first place). These links make it possible to enable a challenge of the desired type or disable it.', array('%admincaptcha' => 'administer CAPTCHA settings')),
  );
  // field set with form_id -> CAPTCHA type configuration
  $form['captcha_types'] = array(
    '#type' => 'fieldset',
    '#title' => t('Challenge type per form'),
    '#description' => t('Select the challenge type you want for each of the listed forms (identified by their so called <em>form_id</em>\'s). You can easily add arbitrary forms with the help of the \'%CAPTCHA_admin_links\' option.', array('%CAPTCHA_admin_links' => t('Add CAPTCHA administration links to forms'))),
    '#tree' => TRUE,
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#theme' => 'captcha_admin_settings_captcha_points',
  );
  // list all possible form_id's
  $captcha_types = _captcha_available_challenge_types();
  $result = db_query("SELECT * FROM {captcha_points} ORDER BY form_id");
  while ($captcha_point = db_fetch_object($result)) {
    $form['captcha_types'][$captcha_point->form_id]['form_id'] = array(
      '#value' => $captcha_point->form_id,
    );
    // select widget for CAPTCHA type
    $form['captcha_types'][$captcha_point->form_id]['captcha_type'] = array(
      '#type' => 'select',
      '#default_value' => "{$captcha_point->module}/{$captcha_point->type}",
      '#options' => $captcha_types,
    );
    // additional operations
    $form['captcha_types'][$captcha_point->form_id]['operations'] = array(
      '#value' => implode(", ", array(
        l(t('delete'), "admin/user/captcha/{$captcha_point->form_id}/delete"),
      ))
    );
  }
  // field(s) for setting the additional CAPTCHA description
  if (module_exists('locale')) {
    $langs = locale_language_list();
    $form['captcha_descriptions'] = array(
      '#type' => 'fieldset',
      '#title' => t('Challenge description'),
      '#description' => t('With this description you can explain the purpose of the challenge to the user.'),
    );
    foreach ($langs as $lang_code => $lang_name) {
      $form['captcha_descriptions']["captcha_description_$lang_code"] = array(
        '#type' => 'textfield',
        '#title' => t('For language %lang_name (code %lang_code)', array('%lang_name' => $lang_name, '%lang_code' => $lang_code)),
        '#default_value' => _captcha_get_description($lang_code),
        '#maxlength' => 256,
      );
    }
  }
  else {
    $form['captcha_description'] = array(
      '#type' => 'textfield',
      '#title' => t('Challenge description'),
      '#description' => t('With this description you can explain the purpose of the challenge to the user.'),
      '#default_value' => _captcha_get_description(),
      '#maxlength' => 256,
    );
  }
  // field for CAPTCHA persistence
  $form['captcha_persistence'] = array(
    '#type' => 'radios',
    '#title' => t('Persistence'),
    '#default_value' => variable_get('captcha_persistence', CAPTCHA_PERSISTENCE_SHOW_ALWAYS),
    '#options' => array(
      CAPTCHA_PERSISTENCE_SHOW_ALWAYS => t('Always add a challenge.'),
      CAPTCHA_PERSISTENCE_SKIP_ONCE_SUCCESSFUL_PER_FORM => t('Omit challenges for a form once the user has successfully responded to a challenge for that form.'),
      CAPTCHA_PERSISTENCE_SKIP_ONCE_SUCCESSFUL => t('Omit challenges for all forms once the user has successfully responded to a challenge.'),
    ),
    '#description' => t('Define if challenges should be omitted during the rest of a session once the user successfully responses to a challenge.'),
  );
  // option for logging wrong responses
  $form['captcha_log_wrong_responses'] = array(
    '#type' => 'checkbox',
    '#title' => t('Log wrong responses'),
    '#description' => t('Report information about wrong responses to the !log.', array('!log' => l('log', 'admin/reports/dblog'))),
    '#default_value' => variable_get('captcha_log_wrong_responses', FALSE),
  );
  // submit button
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  return $form;
}

/**
 * Custom theme function for a table of (form_id -> CAPTCHA type) settings
 */
function theme_captcha_admin_settings_captcha_points($form) {
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = drupal_render($form[$key]['form_id']);
    $row[] = drupal_render($form[$key]['captcha_type']);
    $row[] = drupal_render($form[$key]['operations']);
    $rows[] = $row;
  }
  $header = array('form_id', t('Challenge type (module)'), t('Operations'));
  $output = theme('table', $header, $rows);
  return $output;
}

/**
 * Submission function for captcha_admin_settings form
 */
function captcha_admin_settings_submit($form, &$form_state) {
  if ($form['form_id']['#value'] == 'captcha_admin_settings') {
    variable_set('captcha_administration_mode', $form_state['values']['captcha_administration_mode']);
    foreach ($form_state['values']['captcha_types'] as $captcha_point_form_id => $data) {
      if ($data['captcha_type'] == 'none') {
        db_query("UPDATE {captcha_points} SET module = NULL, type = NULL WHERE form_id = '%s'", $captcha_point_form_id);
      }
      else {
        list($module, $type) = explode('/', $data['captcha_type']);
        db_query("UPDATE {captcha_points} SET module = '%s', type = '%s' WHERE form_id = '%s'", $module, $type, $captcha_point_form_id);
      }
    }
    // description stuff
    if (module_exists('locale')) {
      $langs = locale_language_list();
      foreach ($langs as $lang_code => $lang_name) {
        variable_set("captcha_description_$lang_code", $form_state['values']["captcha_description_$lang_code"]);
      }
    }
    else {
      variable_set('captcha_description', $form_state['values']['captcha_description']);
    }
    variable_set('captcha_persistence', $form_state['values']['captcha_persistence']);
    variable_set('captcha_log_wrong_responses', $form_state['values']['captcha_log_wrong_responses']);
    drupal_set_message(t('The CAPTCHA settings were saved.'), 'status');
  }
}

/**
 * form generating function for CAPTCHA point settings
 */
function captcha_captcha_point_settings_form($form_state, $form_id) {
  $result = db_query("SELECT * FROM {captcha_points} WHERE form_id = '%s'", $form_id);
  $captcha_point = db_fetch_object($result);
  if ($captcha_point) {
    $form = array();
    $form['captcha_point_form_id'] = array(
      '#type' => 'hidden',
      '#value' => $captcha_point->form_id,
    );
    // select widget for CAPTCHA type
    $form['captcha_type'] = array(
      '#type' => 'select',
      '#title' => t('Select the challenge for @form_id', array('@form_id' => $form_id)),
      '#default_value' => "{$captcha_point->module}/{$captcha_point->type}",
      '#options' => _captcha_available_challenge_types(),
    );
    // submit button
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
    // cancel button
    $form['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Cancel'),
    );
    return $form;
  }
  else {
    // illegal form_id
    drupal_set_message(t('Unavailable form_id %form_id ', array('%form_id' => $form_id)), 'error');
    // goto the CAPTCHA administration or alternative destination if present in URI
    drupal_goto('admin/user/captcha');
  }
}

/**
 * submit function for captcha_captcha_point_settings_form
 */
function captcha_captcha_point_settings_form_submit($form, &$form_state) {
  if ($form_state['clicked_button']['#value'] == t('Save')) {
    $captcha_point_form_id = $form_state['values']['captcha_point_form_id'];
    $captcha_type = $form_state['values']['captcha_type'];
    if ($captcha_type == 'none') {
      db_query("UPDATE {captcha_points} SET module = NULL, type = NULL WHERE form_id = '%s'", $captcha_point_form_id);
    }
    else {
      list($module, $type) = explode('/', $captcha_type);
      db_query("UPDATE {captcha_points} SET module = '%s', type = '%s' WHERE form_id = '%s'", $module, $type, $captcha_point_form_id);
    }
    drupal_set_message(t('Saved CAPTCHA settings.'), 'status');
  }
}

/**
 * Funtion for generating a page with CAPTCHA examples
 * If the arguments $module and $challenge are not set, generate a list with
 * examples of the available CAPTCHA types.
 * If $module and $challenge are set, generate 10 examples of the concerning
 * CAPTCHA.
 */
function captcha_examples($form_state, $module, $challenge) {
  $form = array();
  if ($module && $challenge) {
    // generate 10 examples
    for ($i=0; $i<10; $i++) {
      // generate CAPTCHA
      $captcha = call_user_func_array($module .'_captcha', array('generate', $challenge));
      // add form elements
      $form["challenge_{$i}"] = $captcha['form'];
    }
  }
  else {
    // generate a list with examples of the available CAPTCHA types
    $form['info'] = array(
      '#value' => t('This page gives an overview of all available challenge types, generated with their current settings.'),
    );
    foreach (module_implements('captcha') as $mkey => $module) {
      $challenges = call_user_func_array($module .'_captcha', 'list');
      if ($challenges) {
        foreach ($challenges as $ckey => $challenge) {
          // generate CAPTCHA
          $captcha = call_user_func_array($module .'_captcha', array('generate', $challenge));
          // build form
          $form["captcha_{$mkey}_{$ckey}"] = array(
            '#type' => 'fieldset',
            '#title' => t('Challenge "%challenge" by module "%module"', array('%challenge' => $challenge, '%module' => $module)),
            'challenge' => $captcha['form'],
            'more_examples' => array(
              '#value' => l(t('10 more examples of this challenge.'), "admin/user/captcha/captcha/examples/$module/$challenge"),
            ),
          );
        }
      }
    }
  }
  return $form;
}
