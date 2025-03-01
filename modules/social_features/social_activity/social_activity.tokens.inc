<?php

/**
 * @file
 * Builds placeholder replacement tokens for Social Activity module.
 */

use Drupal\Core\Render\BubbleableMetadata;
use Drupal\Core\StringTranslation\TranslatableMarkup;
use Drupal\group\Entity\GroupRelationshipInterface;
use Drupal\message\Entity\Message;
use Drupal\node\NodeInterface;
use Drupal\user\EntityOwnerInterface;

/**
 * Implements hook_token_info().
 */
function social_activity_token_info() {
  // The token additional information has been deprecated in 10.x.
  // It will be removed from 11.x.
  $tokens['additional_information'] = [
    'name' => t('Additional information.'),
    'description' => t('Additional message information. This token is deprecated, please use "preview" and "cta_button" tokens instead in your message templates.'),
  ];

  $tokens['preview'] = [
    'name' => t('Preview.'),
    'description' => t('Preview of the related entity.'),
  ];

  $tokens['cta_button'] = [
    'name' => t('CTA button'),
    'description' => t('A call to action button.'),
  ];

  $tokens['count_groups_per_node'] = [
    'name' => t('Groups count per node.'),
    'description' => t('Number of groups where node is added.'),
  ];

  $tokens['revision_author'] = [
    'name' => t("Author"),
    'type' => 'user',
  ];

  return [
    'tokens' => [
      'message' => $tokens,
    ],
  ];
}

/**
 * Implements hook_tokens().
 */
function social_activity_tokens($type, $tokens, array $data, array $options, BubbleableMetadata $bubbleable_metadata) {
  $replacements = [];

  if ($type === 'message' && !empty($data['message'])) {
    // Translate hook_tokens options to the options for TranslatableMarkup.
    $translation_options = [
      'langcode' => $options['langcode'] ?? NULL,
    ];

    /** @var \Drupal\message\Entity\Message $message */
    $message = $data['message'];

    if (
      ($message instanceof Message) &&
      $message->hasField('field_message_related_object') &&
      !$message->get('field_message_related_object')->isEmpty()
    ) {
      /** @var \Drupal\social_activity\EmailTokenServices $email_token_services */
      $email_token_services = \Drupal::service('social_activity.email_token_services');

      $entity = $email_token_services->getRelatedObject($message);
      if ($entity) {
        $date_formatter = \Drupal::service('date.formatter');
        $date_format = 'social_long_date';
        $summary = '';

        if ($entity->getEntityTypeId() === 'post') {
          /** @var \Drupal\social_post\Entity\Post $entity */

          // Prepare the link to post.
          $link = $entity->toUrl('canonical', ['absolute' => TRUE]);

          // Prepare the preview information.
          $preview_info = $email_token_services->getPostPreview($entity);

          // Prepare CTA button information.
          $cta_button = $email_token_services->getCtaButton($link, new TranslatableMarkup('Leave a comment', [], $translation_options));

          // This is for token deprecated token 'additional_information'.
          $additional_information = [
            '#theme' => 'message_post_teaser',
            '#name' => $entity->getOwner()->getDisplayName(),
            '#date' => $date_formatter->format($entity->getCreatedTime(), $date_format),
            '#summary' => $summary,
            '#link' => $link,
          ];
        }
        elseif ($entity->getEntityTypeId() === 'group_content' && $entity instanceof GroupRelationshipInterface) {
          /** @var \Drupal\node\Entity\Node $node */
          $node = $entity->getEntity();
          if ($node instanceof NodeInterface) {
            $preview_info = $email_token_services->getContentPreview($node);

            // Prepare the link to node.
            $link = $node->toUrl('canonical', ['absolute' => TRUE]);

            // Prepare the CTA button markup.
            $cta_button = $email_token_services->getCtaButton($link, new TranslatableMarkup('Read more', [], $translation_options));

            // This is for token deprecated token 'additional_information'.
            $additional_information = [
              '#theme' => 'message_node_teaser',
              '#link' => $link,
              '#type' => strtoupper($node->getType()),
            ];
          }
        }

        // Replace tokens.
        foreach ($tokens as $name => $original) {
          switch ($name) {
            // The token 'additional_information'̊ is deprecated and
            // will be removed in 11.x release of Open Social.
            case 'additional_information':
              if (!empty($additional_information)) {
                $replacements[$original] = \Drupal::service('renderer')->renderInIsolation($additional_information);
              }
              break;

            case 'count_groups_per_node':
              // For the moment we can count only for group_content
              // and node entities.
              $count = \Drupal::service('social_group.cross_posting')
                ->countGroupsByGroupContentNode($entity);
              // Recount as token renders in group context, and we need to
              // exclude the one.
              if ($count > 0) {
                $count--;
              }
              // Build the string like "(+ 4 groups more)".
              $replacements[$original] = $count > 0
                ? \Drupal::translation()->formatPlural($count, '(+ @count other groups)', '(+ @count other groups)')
                : '';
              break;

            case 'preview':
              if (!empty($preview_info)) {
                $replacements[$original] = \Drupal::service('renderer')->renderInIsolation($preview_info);
              }
              break;

            case 'cta_button':
              if (!empty($cta_button)) {
                $replacements[$original] = \Drupal::service('renderer')->renderInIsolation($cta_button);
              }
              break;
          }
        }

        // Loading user data to user token as "object".
        $token_service = \Drupal::token();
        if ($author_tokens = $token_service->findWithPrefix($tokens, 'revision_author')) {
          if ($message->field_message_related_object->isEmpty()) {
            return $replacements;
          }

          // Get related content to change tokens.
          $related_object_value = current($message->field_message_related_object->getValue());
          $related_object_content = \Drupal::entityTypeManager()
            ->getStorage($related_object_value['target_type'])
            ->load($related_object_value['target_id']);
          if (empty($related_object_content)) {
            return $replacements;
          }

          // Get the Entity when the entity type is group_content.
          // The owner from group_content didn't update, so is outdated.
          if (
            $related_object_content->getEntityTypeId() === 'group_content'
            && method_exists($related_object_content, 'getEntity')
          ) {
            $related_object_content = $related_object_content->getEntity();
          }

          if ($related_object_content instanceof EntityOwnerInterface) {
            $replacements += $token_service->generate('user', $author_tokens, ['user' => $related_object_content->getOwner()], $options, $bubbleable_metadata);
          }

          return $replacements;
        }
      }
    }
  }

  return $replacements;
}
