<?php

namespace Drupal\social_demo;

use Drupal\group\Entity\GroupInterface;

/**
 * Abstract class for demo group creation.
 *
 * @package Drupal\social_demo
 */
abstract class DemoGroup extends DemoContent {

  /**
   * {@inheritdoc}
   */
  public function createContent($generate = FALSE, $max = NULL) {
    $data = $this->fetchData();
    if ($generate === TRUE) {
      $data = $this->scrambleData($data, $max);
    }

    foreach ($data as $uuid => $item) {
      // Must have uuid and same key value.
      if ($uuid !== $item['uuid']) {
        $this->loggerChannelFactory->get('social_demo')->error("Group with uuid: {$uuid} has a different uuid in content.");
        continue;
      }

      // Check whether group with same uuid already exists.
      $groups = $this->entityStorage->loadByProperties([
        'uuid' => $uuid,
      ]);

      if ($groups) {
        $this->loggerChannelFactory->get('social_demo')->warning("Group with uuid: {$uuid} already exists.");
        continue;
      }

      // Try to load a user account (author's account).
      $account = $this->loadByUuid('user', $item['uid']);

      if (!$account) {
        $this->loggerChannelFactory->get('social_demo')->error("Account with uuid: {$item['uid']} doesn't exists.");
        continue;
      }

      // Create array with data of a group.
      $item['uid'] = $account->id();
      $item['created'] = $item['changed'] = $this->createDate($item['created']);

      // Load image by uuid and set to a group.
      if (!empty($item['image'])) {
        $item['image'] = $this->prepareImage($item['image'], $item['image_alt']);
      }
      else {
        // Set "null" to exclude errors during saving
        // (in cases when image will equal  to "false").
        $item['image'] = NULL;
      }

      // Attach key documents.
      if (!empty($item['files'])) {
        $item['files'] = $this->prepareFiles($item['files']);
      }
      else {
        // Set "null" to exclude errors during saving
        // (in cases when array with files will empty).
        $item['files'] = NULL;
      }

      $entry = $this->getEntry($item);
      $entity = $this->entityStorage->create($entry);
      $entity->save();

      if (!$entity->id()) {
        continue;
      }

      $this->content[$entity->id()] = $entity;

      if (!empty($item['members'])) {
        $managers = !empty($item['managers']) ? $item['managers'] : [];
        $this->addMembers($item['members'], $managers, $entity);
      }
    }

    return $this->content;
  }

  /**
   * {@inheritdoc}
   */
  protected function getEntry(array $item) {
    $entry = [
      'uuid' => $item['uuid'],
      'langcode' => $item['langcode'],
      'type' => $item['type'],
      'label' => $item['label'],
      'field_group_description' => [
        [
          'value' => $item['description'],
          'format' => 'basic_html',
        ],
      ],
      'uid' => $item['uid'],
      'created' => $item['created'],
      'changed' => $item['changed'],
      'field_group_image' => $item['image'],
      'field_group_files' => $item['files'],
      'field_flexible_group_visibility' => $item['field_flexible_group_visibility'],
      'field_group_allowed_join_method' => $item['field_group_allowed_join_method'],
      'field_group_allowed_visibility' => $item['field_group_allowed_visibility'],
    ];

    return $entry;
  }

  /**
   * Converts a date in the correct format.
   *
   * @param string $date_string
   *   The date.
   *
   * @return int|false
   *   Returns a timestamp on success, false otherwise.
   */
  protected function createDate($date_string) {
    // Split from delimiter.
    $timestamp = explode('|', $date_string);

    $date = strtotime($timestamp[0]);
    $date = date('Y-m-d', $date) . 'T' . $timestamp[1] . ':00';

    return strtotime($date);
  }

  /**
   * Adds members to a group.
   *
   * @param array $members
   *   The array of members.
   * @param array $managers
   *   A list of group managers.
   * @param \Drupal\group\Entity\GroupInterface $entity
   *   The GroupInterface entity.
   */
  protected function addMembers(array $members, array $managers, GroupInterface $entity) {
    foreach ($members as $account_uuid) {
      $account = $this->userStorage->loadByProperties([
        'uuid' => $account_uuid,
      ]);

      if (($account = current($account)) && !$entity->getMember($account)) {
        $values = [];
        // If the user should have the manager role, grant it to them now.
        if (in_array($account_uuid, $managers)) {
          $values = ['group_roles' => [$entity->bundle() . '-group_manager']];
        }
        $entity->addMember($account, $values);
      }
    }
  }

  /**
   * Prepares an array with list of files to set as field value.
   *
   * @param string $files
   *   The uuid for the file.
   *
   * @return array
   *   Returns an array.
   */
  protected function prepareFiles($files) {
    $values = [];

    foreach ($files as $file_uuid) {
      $file = $this->fileStorage->loadByProperties([
        'uuid' => $file_uuid,
      ]);

      if ($file) {
        $values[] = [
          'target_id' => current($file)->id(),
        ];
      }
    }

    return $values;
  }

}
