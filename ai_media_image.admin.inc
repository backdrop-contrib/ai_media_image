<?php

/**
 * @file
 * Administration and generation forms for AI Media Image.
 */

/**
 * Form builder for generating AI images into the media library.
 */
function ai_media_image_generate_form($form, &$form_state) {
  $image_models = [];
  if (function_exists('ai_get_all_image_models')) {
    $image_models = ai_get_all_image_models();
  }

  // Flatten or group options for select widget.
  $model_options = [];
  if (!empty($image_models)) {
    foreach ($image_models as $provider => $models) {
      if (is_array($models)) {
        foreach ($models as $id => $label) {
          $model_options[$provider][$id] = $label;
        }
      }
      else {
        $model_options[$provider] = $models;
      }
    }
  }

  if (empty($model_options)) {
    backdrop_set_message(t('No image-generation models are currently available. Please configure an image-capable provider (such as OpenAI or Azure with DALL-E) under <a href="@url">AI Providers</a>.', [
      '@url' => url('admin/config/ai/settings'),
    ]), 'warning');
  }

  $form['prompt'] = [
    '#type' => 'textarea',
    '#title' => t('Image Prompt'),
    '#description' => t('Describe the image you want the AI model to generate in detail.'),
    '#required' => TRUE,
    '#rows' => 4,
    '#attributes' => [
      'placeholder' => t('A futuristic aeromodeling club workshop with sleek balsa wood gliders, warm natural lighting, digital blueprints on holographic screens...'),
    ],
  ];

  $form['settings'] = [
    '#type' => 'fieldset',
    '#title' => t('Generation Settings'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  ];

  $form['settings']['model'] = [
    '#type' => 'select',
    '#title' => t('Image Model'),
    '#options' => $model_options,
    '#required' => TRUE,
    '#empty_option' => t('- Select an image model -'),
    '#default_value' => config_get('ai.settings', 'default_image_model') ?: NULL,
  ];

  // Shapes rather than pixel sizes: each provider adapter maps the ratio to
  // the closest size or aspect ratio its API accepts.
  $form['settings']['aspect_ratio'] = [
    '#type' => 'select',
    '#title' => t('Shape'),
    '#options' => [
      '1:1' => t('Square (1:1)'),
      '3:2' => t('Landscape (3:2)'),
      '2:3' => t('Portrait (2:3)'),
      '16:9' => t('Widescreen (16:9)'),
      '9:16' => t('Tall (9:16)'),
    ],
    '#default_value' => '1:1',
    '#description' => t('Models that do not offer this exact shape use the closest one they support.'),
  ];

  $form['settings']['filename'] = [
    '#type' => 'textfield',
    '#title' => t('Custom Filename (Optional)'),
    '#description' => t('A name for the file, without extension (e.g., <code>banner-hero</code>). The extension matches the image format the model returns. Leave blank to generate one from the prompt.'),
    '#default_value' => '',
    '#maxlength' => 64,
  ];

  if (module_exists('ai_alt')) {
    $form['settings']['generate_alt'] = [
      '#type' => 'checkbox',
      '#title' => t('Automatically generate accessible alt text using AI'),
      '#default_value' => TRUE,
      '#description' => t('Runs the generated image through vision analysis to produce alt text upon save.'),
    ];
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Generate & Save to Media Library'),
    '#button_type' => 'primary',
  ];
  $form['actions']['cancel'] = [
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => 'admin/content/files',
  ];

  return $form;
}

/**
 * Generates alt text for a saved image file via ai_alt.
 *
 * The alt text is stored on the file entity's global alt field (the field
 * image_entity_alt manages, the same one ai_alt_bulk writes to).
 */
function _ai_media_image_generate_alt($file) {
  if (!function_exists('ai_alt_generate_from_file')) {
    return;
  }

  $alt_field = function_exists('image_entity_alt_field_name') ? image_entity_alt_field_name() : 'field_alt';
  if (!field_info_instance('file', $alt_field, $file->type)) {
    backdrop_set_message(t('Alt text was not generated: image files have no %field field to store it in.', ['%field' => $alt_field]), 'warning');
    return;
  }

  $alt_text = ai_alt_generate_from_file($file);
  if (!$alt_text) {
    backdrop_set_message(t('The image was saved, but alt text could not be generated.'), 'warning');
    return;
  }

  $file->{$alt_field}[LANGUAGE_NONE][0]['value'] = $alt_text;
  file_save($file);
}

/**
 * Validation handler for ai_media_image_generate_form.
 */
function ai_media_image_generate_form_validate($form, &$form_state) {
  $prompt = trim($form_state['values']['prompt']);
  if ($prompt === '') {
    form_set_error('prompt', t('Please provide a prompt describing the image to generate.'));
  }

  $model = $form_state['values']['model'];
  $available = FALSE;
  foreach (ai_get_all_image_models() as $models) {
    if (is_array($models) ? isset($models[$model]) : $models === $model) {
      $available = TRUE;
      break;
    }
  }
  if (!$available) {
    form_set_error('model', t('The selected image model is not available.'));
  }
}

/**
 * Submit handler for ai_media_image_generate_form.
 */
function ai_media_image_generate_form_submit($form, &$form_state) {
  $prompt = trim($form_state['values']['prompt']);
  $model = $form_state['values']['model'];
  $aspect_ratio = $form_state['values']['aspect_ratio'];

  $stripped_model = '';
  $api = ai_get_api_for_model($model, $stripped_model);
  if (!$api) {
    backdrop_set_message(t('Could not obtain an AI API client for model @model.', ['@model' => $model]), 'error');
    return;
  }

  try {
    // b64_json is a preference only; adapters that cannot honor it return a
    // URL or data URL, which AIImageHelper::extract() also handles.
    $response = $api->images($stripped_model, $prompt, $aspect_ratio, 'b64_json');
    try {
      $image = AIImageHelper::extract($response);
    }
    catch (\RuntimeException $e) {
      watchdog('ai_media_image', 'Unusable image response from @model (shape: @shape): @message', [
        '@model' => $model,
        '@shape' => AIImageHelper::describe($response),
        '@message' => $e->getMessage(),
      ], WATCHDOG_ERROR);
      throw $e;
    }

    // Only a sanitized basename is taken from the user; the extension always
    // comes from the validated image type.
    $basename = preg_replace('/[^a-z0-9_-]+/i', '-', pathinfo(trim($form_state['values']['filename']), PATHINFO_FILENAME));
    $basename = trim($basename, '-_');
    if ($basename === '') {
      $slug = trim(preg_replace('/[^a-z0-9]+/i', '-', strtolower(substr($prompt, 0, 32))), '-');
      $basename = 'ai-' . ($slug ?: 'image') . '-' . REQUEST_TIME;
    }
    $filename = substr($basename, 0, 64) . '.' . $image['ext'];

    // The site's default scheme, so generated images live with the rest of
    // the file library (public, s3, etc.).
    $directory = file_default_scheme() . '://ai-media-images';
    file_prepare_directory($directory, FILE_CREATE_DIRECTORY);
    $destination = $directory . '/' . $filename;

    $file = file_save_data($image['data'], $destination, FILE_EXISTS_RENAME);
    if (!$file || empty($file->fid)) {
      backdrop_set_message(t('Failed to save the generated image to Backdrop file storage.'), 'error');
      return;
    }

    // Ensure type is image.
    $file->type = 'image';
    file_save($file);

    if (!empty($form_state['values']['generate_alt'])) {
      _ai_media_image_generate_alt($file);
    }

    watchdog('ai_media_image', 'Generated AI image @filename (fid: @fid) using model @model.', [
      '@filename' => $file->filename,
      '@fid' => $file->fid,
      '@model' => $model,
    ], WATCHDOG_INFO);

    backdrop_set_message(t('Successfully generated AI image and saved to media library: <a href="@url">@filename</a>.', [
      '@url' => url('file/' . $file->fid),
      '@filename' => $file->filename,
    ]));

    $form_state['redirect'] = 'file/' . $file->fid;
  }
  catch (\Exception $e) {
    watchdog('ai_media_image', 'Failed to generate image: @message', ['@message' => $e->getMessage()], WATCHDOG_ERROR);
    backdrop_set_message(t('Error generating image: @message', ['@message' => $e->getMessage()]), 'error');
  }
}
