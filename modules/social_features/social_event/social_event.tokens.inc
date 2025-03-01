<?php

/**
 * @file
 * Builds placeholder replacement tokens for Social Event module.
 */

use Drupal\Core\Render\BubbleableMetadata;
use Drupal\Core\StringTranslation\TranslatableMarkup;
use Drupal\Core\Link;
use Drupal\Core\Url;
use Drupal\message\Entity\Message;
use Drupal\node\NodeInterface;
use Drupal\social_event\EventEnrollmentInterface;

/**
 * Implements hook_token_info().
 */
function social_event_token_info() {

  $type = [
    'name' => t('Social Event'),
    'description' => t('Tokens from the Social Event module.'),
  ];

  $social_event['event_iam_organizing'] = [
    'name' => t('URL of the Event I organize.'),
    'description' => t('URL of the Event I organize.'),
  ];

  $social_event['enrolled_user'] = [
    'name' => t('Username of user enrolled to the event.'),
    'description' => t('Username of user enrolled to the Event. Can be an anonymous user.'),
  ];

  $social_event['enrolled_event_url'] = [
    'name' => t('Url of the Event a user enrolled to.'),
    'description' => t('Url of the Event a user enrolled to.'),
  ];

  $social_event['enrolled_users_count'] = [
    'name' => t('Enrolled users count'),
    'description' => t('The total count of enrolled users to the current event.'),
  ];

  return [
    'types' => ['social_event' => $type],
    'tokens' => [
      'social_event' => $social_event,
    ],
  ];
}

/**
 * Implements hook_tokens().
 */
function social_event_tokens($type, $tokens, array $data, array $options, BubbleableMetadata $bubbleable_metadata) {
  $replacements = [];
  if ($type === 'social_event') {
    foreach ($tokens as $name => $original) {
      switch ($name) {
        case 'enrolled_users_count':
          $nid = Drupal::routeMatch()->getRawParameter('node');
          if ($nid) {
            $node = \Drupal::entityTypeManager()
              ->getStorage('node')
              ->load($nid);
            if ($node instanceof NodeInterface && $node->getType() === 'event') {
              $enrollments = \Drupal::service('social_event.status_helper');
              $count = count($enrollments->getEventEnrollmentsByStatus((int) $node->id(), [
                EventEnrollmentInterface::INVITE_ACCEPTED_AND_JOINED,
                EventEnrollmentInterface::REQUEST_APPROVED,
              ], TRUE));
              $enrollments = \Drupal::translation()->formatPlural(
                $count,
                ':count person has enrolled',
                ':count people have enrolled',
                [':count' => $count]
              );
              if ($count !== NULL) {
                $replacements[$original] = $enrollments;
              }
            }
          }
          break;
      }
    }
  }
  if ($type === 'social_event' && !empty($data['message'])) {
    /** @var Drupal\message\Entity\Message $message */
    $message = $data['message'];

    if ($message instanceof Message) {
      $event_enrollment_id = $message->getFieldValue('field_message_related_object', 'target_id');

      foreach ($tokens as $name => $original) {
        switch ($name) {
          case 'event_iam_organizing':
            if (!empty($event_enrollment_id)) {
              $link = _social_event_get_link_to_event_from_enrollment($event_enrollment_id);
              if ($link !== NULL) {
                $replacements[$original] = $link;
              }
            }
            break;

          case 'enrolled_event_url':
            if (!empty($event_enrollment_id)) {
              $link = _social_event_get_link_to_event_from_enrollment($event_enrollment_id, TRUE);
              if ($link !== NULL) {
                $replacements[$original] = $link;
              }
            }
            break;

          case 'enrolled_user':
            if (!empty($event_enrollment_id)) {
              $enrollment_name = _social_event_get_name_from_enrollment($event_enrollment_id);

              if ($enrollment_name !== NULL) {
                $replacements[$original] = $enrollment_name;
              }
            }
            break;
        }
      }
    }
  }

  return $replacements;
}

/**
 * Implements hook_tokens_alter().
 */
function social_event_tokens_alter(array &$replacements, array $context, BubbleableMetadata $bubbleable_metadata) {
  if ($context['type'] == 'message' && !empty($context['data']['message'])) {
    // Translate hook_tokens options to the options for TranslatableMarkup.
    $translation_options = [
      'langcode' => $context['options']['langcode'] ?? NULL,
    ];

    /** @var Drupal\message\Entity\Message $message */
    $message = $context['data']['message'];

    // Alter the [message:cta_button] token, and [message:cta_button]
    // and replace it with the rendered content with new text and link.
    if (isset($context['tokens']['cta_button'])|| isset($context['tokens']['preview'])) {
      if ($message->hasField('field_message_related_object') && !$message->get('field_message_related_object')->isEmpty()) {
        /** @var \Drupal\social_activity\EmailTokenServices $email_token_services */
        $email_token_services = \Drupal::service('social_activity.email_token_services');

        $target_type = $message->getFieldValue('field_message_related_object', 'target_type');

        if ($target_type === 'event_enrollment' && $event_enrollment_id = $message->getFieldValue('field_message_related_object', 'target_id')) {
          if (isset($context['tokens']['cta_button'])) {
            $event = _social_event_get_event_from_enrollment($event_enrollment_id);
            if ($event !== NULL) {
              $message_template_id = $message->getTemplate()->id();
              switch ($message_template_id) {
                case 'activity_on_events_im_organizing':
                  $link = Url::fromRoute('view.event_manage_enrollments.page_manage_enrollments', ['node' => $event->id()]);
                  $cta_button = $email_token_services->getCtaButton($link, new TranslatableMarkup('View enrollments', [], $translation_options));
                  $replacements[$context['tokens']['cta_button']] = \Drupal::service('renderer')
                    ->renderInIsolation($cta_button);
                  break;

                case 'request_event_enrollment':
                  $link = Url::fromRoute('view.event_manage_enrollment_requests.page_manage_enrollment_requests', ['node' => $event->id()]);
                  $cta_button = $email_token_services->getCtaButton($link, new TranslatableMarkup('View enrollment requests', [], $translation_options));
                  $replacements[$context['tokens']['cta_button']] = \Drupal::service('renderer')
                    ->renderInIsolation($cta_button);
                  break;

                case 'member_added_by_event_organiser':
                case 'event_request_approved':
                  $link = $event->toUrl('canonical');
                  $cta_button = $email_token_services->getCtaButton($link, new TranslatableMarkup('See the event', [], $translation_options));
                  $replacements[$context['tokens']['cta_button']] = \Drupal::service('renderer')
                    ->renderInIsolation($cta_button);
                  break;

                case 'user_was_enrolled_to_event':
                  $link = $event->toUrl();
                  $cta_button = $email_token_services->getCtaButton($link, new TranslatableMarkup('See event details', [], $translation_options));
                  $replacements[$context['tokens']['cta_button']] = \Drupal::service('renderer')
                    ->renderInIsolation($cta_button);
                  break;

              }
            }
          }
          // Replace the preview token.
          if (isset($context['tokens']['preview'])) {
            $event = _social_event_get_event_from_enrollment($event_enrollment_id);
            if ($event !== NULL) {
              /** @var \Drupal\node\Entity\Node $event */
              $view_builder = \Drupal::entityTypeManager()->getViewBuilder('node');
              $preview_info = $view_builder->view($event, 'email_card');
              $renderer = \Drupal::service('renderer');
              $renderer->addCacheableDependency($preview_info, $translation_options);
              $replacements[$context['tokens']['preview']] = $renderer->renderInIsolation($preview_info);
            }
          }
        }
      }
    }
  }
}

/**
 * Render a link to an event as full link with title or just absolute as string.
 *
 * @param int $event_enrollment_id
 *   The ID of the event enrollment.
 * @param bool $as_link
 *   True if you want the full link otherwise just a canonical URL string.
 *
 * @return string|null
 *   the Url.
 *
 * @throws \Drupal\Component\Plugin\Exception\InvalidPluginDefinitionException
 * @throws \Drupal\Component\Plugin\Exception\PluginNotFoundException
 * @throws \Drupal\Core\Entity\EntityMalformedException
 */
function _social_event_get_link_to_event_from_enrollment(int $event_enrollment_id, bool $as_link = FALSE) {
  // Check if the event still exists.
  $event = _social_event_get_event_from_enrollment($event_enrollment_id);
  if ($event && !is_null($event->getTitle())) {
    if ($as_link) {
      $url_string = $event->toUrl('canonical')->toString(TRUE);
      return $url_string->getGeneratedUrl();
    }
    else {
      return Link::fromTextAndUrl($event->getTitle(), $event->toUrl('canonical'))->toString();
    }
  }
  else {
    return NULL;
  }
}

/**
 * Get the user display name from an Enrollment also for AN.
 *
 * @param int $id
 *   the Enrollment entity ID.
 *
 * @return array|\Drupal\Component\Render\MarkupInterface|mixed|null|string
 *   Markup for the Username.
 *
 * @throws \Drupal\Component\Plugin\Exception\InvalidPluginDefinitionException
 * @throws \Drupal\Component\Plugin\Exception\PluginNotFoundException
 */
function _social_event_get_name_from_enrollment($id) {
  $entity_storage = \Drupal::entityTypeManager()
    ->getStorage('event_enrollment');
  /** @var \Drupal\social_event\Entity\EventEnrollment $enrollment */
  $enrollment = $entity_storage->load($id);

  // User is AN by default.
  $enrollment_name = \Drupal::configFactory()->get('user.settings')->get('anonymous');

  if ($enrollment instanceof EventEnrollmentInterface) {
    // If there is a Uid. Lets load the user and return their display name.
    if ($enrollment !== NULL &&
      $enrollment->hasField('field_account') &&
      $enrollment->getFieldValue('field_account', 'target_id') > 0) {
      $entity_storage = \Drupal::entityTypeManager()
        ->getStorage('user');
      /** @var \Drupal\user\Entity\User $user */
      $user = $entity_storage->load($enrollment->getFieldValue('field_account', 'target_id'));
      return $user->getDisplayName();
    }
  }

  // User is AN but has enrollment settings available, lets see if we can get
  // the Firstname and lastname to show up in the notification.
  if ($enrollment !== NULL &&
    $enrollment->hasField('field_first_name') &&
    $enrollment->getFieldValue('field_first_name', 'value') !== NULL &&
    $enrollment->getFieldValue('field_last_name', 'value') !== NULL) {
    // Create the Name from AN Attendees if possible.
    $enrollment_name = $enrollment->getFieldValue('field_first_name', 'value') .
      ' ' . $enrollment->getFieldValue('field_last_name', 'value');
  }

  return $enrollment_name;
}

/**
 * Gets the event object from event enrollment ID.
 *
 * @param int $event_enrollment_id
 *   Event enrollment ID.
 *
 * @return \Drupal\node\Entity\Node|null
 *   Event object.
 *
 * @throws \Drupal\Component\Plugin\Exception\InvalidPluginDefinitionException
 * @throws \Drupal\Component\Plugin\Exception\PluginNotFoundException
 */
function _social_event_get_event_from_enrollment(int $event_enrollment_id) {
  $entity_storage = \Drupal::entityTypeManager()
    ->getStorage('event_enrollment');
  if ($entity = $entity_storage->load($event_enrollment_id)) {
    /** @var \Drupal\social_event\Entity\EventEnrollment $entity */
    $event_id = $entity->getFieldValue('field_event', 'target_id');
    $storage = \Drupal::entityTypeManager()->getStorage('node');
    /** @var \Drupal\node\Entity\Node $event */
    $event = $storage->load($event_id);
  }
  return $event ?? NULL;
}
