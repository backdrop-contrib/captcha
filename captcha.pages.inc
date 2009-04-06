<?php
// $Id$

/**
 * Implementation of a part of hook_form_alter for untrusted users.
 */
function _captcha_form_alter_untrusted_user(&$form, $form_state, $form_id, $captcha_point) {

  $captcha_element = array(
    '#type' => 'captcha',
    '#captcha_type' => $captcha_point->module .'/'. $captcha_point->type,
  );
  $captcha_placement = _captcha_get_captcha_placement($form_id, $form);
  _captcha_insert_captcha_element($form, $captcha_placement, $captcha_element);

}


/**
 * Helper function to get placement information for a given form_id.
 * @param $form_id the form_id to get the placement information for.
 * @param $form if a form corresponding to the given form_id, if there
 *   is no placement info for the given form_id, this form is examined to
 *   guess the placement.
 * @return placement info array (@see _captcha_insert_captcha_element() for more
 *   info about the fields 'path', 'key' and 'weight'.
 */
function _captcha_get_captcha_placement($form_id, $form) {
  // Get CAPTCHA placement map from cache. Two levels of cache:
  // static variable in this function and storage in the variables table.
  static $placement_map = NULL;
  // Try first level cache.
  if ($placement_map === NULL) {
    // If first level cache missed: try second level cache.
    $placement_map = variable_get('captcha_placement_map_cache', NULL);
    // TODO: add UI in admin UI for flushing this cache.

    if ($placement_map === NULL) {
      // If second level cache missed: start from a fresh placement map.
      $placement_map = array();
      // TODO: prefill with some hard coded default entries as follows?
      // $placement_map['comment_form'] = array('path' => array(), 'key' => NULL, 'weight' => 18.9);
      // $placement_map['user_login'] = array('path' => array(), 'key' => NULL, 'weight' => 1.9);
      // TODO: also make the placement 'overridable' from the admin UI?
    }
  }

  // Query the placement map.
  if (array_key_exists($form_id, $placement_map)) {
    $placement = $placement_map[$form_id];
  }
  // If no placement info is available in placement map:
  // search the form for buttons and guess placement from it.
  else {
    $buttons = _captcha_search_buttons($form);
    // TODO: make this more sofisticated? Use cases needed.
    $placement = $buttons[0];
    // Store calculated placement in caches.
    $placement_map[$form_id] = $placement;
    variable_set('captcha_placement_map_cache', $placement_map);
  }
  return $placement;
}

/**
 * Helper function for searching the buttons in a form.
 *
 * @param $form the form to search button elements in
 * @return an array of paths to the buttons.
 *   A path is an array of keys leading to the button, the last
 *   item in the path is the weight of the button element
 *   (or NULL if undefined).
 */
function _captcha_search_buttons($form) {
  $buttons = array();
  foreach (element_children($form) as $key) {
    // Look for submit or button type elements.
    if (isset($form[$key]['#type']) && ($form[$key]['#type'] == 'submit' || $form[$key]['#type'] == 'button')) {
      $weight = isset($form[$key]['#weight']) ? $form[$key]['#weight'] : NULL;
      $buttons[] = array(
        'path' => array(),
        'key' => $key,
        'weight' => $weight,
      );
    }
    // Process children recurively.
    $children_buttons = _captcha_search_buttons($form[$key]);
    foreach ($children_buttons as $b) {
      $b['path'] = array_merge(array($key), $b['path']);
      $buttons[] = $b;
    }
  }
  return $buttons;
}

/**
 * Helper function to insert a CAPTCHA element in a form before a given form element.
 * @param $form the form to add the CAPTCHA element to.
 * @param $placement information where the CAPTCHA element should be inserted.
 *   $target should be an associative array with fields:
 *     - 'path': path of the container in the form where the CAPTCHA element should be inserted.
 *     - 'key': the key of the element before which the CAPTCHA element
 *       should be inserted. If the field 'key' is undefined or NULL, the CAPTCHA will
 *       just be appended to the container.
 *     - 'weight': if 'key' is not NULL: should be the weight of the element defined by 'key'.
 *       If 'key' is NULL and weight is not NULL: set the weight property of the CAPTCHA element
 *       to this value.
 * @param $captcha_element the CAPTCHA element to insert.
 */
function _captcha_insert_captcha_element(&$form, $placement, $captcha_element) {
  // Get common path, target and target weight.
  $target_key = $placement['key'];
  $target_weight = $placement['weight'];
  $path = $placement['path'];

  // Walk through the form along the path.
  $form_stepper = &$form;
  foreach ($path as $step) {
    if (isset($form_stepper[$step])) {
      $form_stepper = & $form_stepper[$step];
    }
    else {
      // Given path is invalid: stop stepping and
      // continue in best effort (append instead of insert).
      $target_key = NULL;
      break;
    }
  }

  // If no target is available: just append the CAPTCHA element to the container.
  if ($target_key == NULL || !array_key_exists($target_key, $form_stepper)) {
    // Optionally, set weight of CAPTCHA element.
    if ($target_weight != NULL) {
      $captcha_element['#weight'] = $target_weight;
    }
    $form_stepper['captcha'] =  $captcha_element;
  }
  // If there is a target available: make sure the CAPTCHA element comes right before it.
  else {
    // If target has a weight: set weight of CAPTCHA element a bit smaller
    // and just append the CAPTCHA: sorting will fix the ordering anyway.
    if ($target_weight != NULL) {
      $captcha_element['#weight'] = $target_weight - .1;
      $form_stepper['captcha'] =  $captcha_element;
    }
    else {
      // If we can't play with weights: insert the CAPTCHA element at the right position.
      // Because PHP lacks a function for this (array_splice() comes close,
      // but it does not preserve the key of the inserted element), we do it by hand:
      // chop of the end, append the CAPTCHA element and put the end back.
      $offset = array_search($target_key, array_keys($form_stepper));
      $end = array_splice($form_stepper, $offset);
      $form_stepper['captcha'] =  $captcha_element;
      foreach($end as $k => $v) {
        $form_stepper[$k] = $v;
      }
    }
  }
}

