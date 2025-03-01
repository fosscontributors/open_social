<?php

namespace Drupal\social_event;

use CloudEvents\V1\CloudEvent;
use Drupal\Component\Uuid\UuidInterface;
use Drupal\Core\Entity\EntityTypeManagerInterface;
use Drupal\Core\Extension\ModuleHandlerInterface;
use Drupal\Core\Routing\RouteMatchInterface;
use Drupal\Core\Session\AccountProxyInterface;
use Drupal\node\NodeInterface;
use Drupal\social_eda\DispatcherInterface;
use Drupal\social_eda\Types\Address;
use Drupal\social_eda\Types\Application;
use Drupal\social_eda\Types\ContentVisibility;
use Drupal\social_eda\Types\DateTime;
use Drupal\social_eda\Types\Entity;
use Drupal\social_eda\Types\Href;
use Drupal\social_eda\Types\User;
use Drupal\social_event\Event\EventEntityData;
use Drupal\user\UserInterface;
use Symfony\Component\HttpFoundation\RequestStack;

/**
 * Handles hook invocations for EDA related operations of the event entity.
 */
final class EdaHandler {

  /**
   * The current logged-in user.
   *
   * @var \Drupal\user\UserInterface|null
   */
  protected ?UserInterface $currentUser = NULL;

  /**
   * The source.
   *
   * @var string
   */
  protected string $source;

  /**
   * The current route name.
   *
   * @var string
   */
  protected string $routeName;

  /**
   * {@inheritDoc}
   */
  public function __construct(
    private readonly DispatcherInterface $dispatcher,
    private readonly UuidInterface $uuid,
    private readonly RequestStack $requestStack,
    private readonly ModuleHandlerInterface $moduleHandler,
    private readonly EntityTypeManagerInterface $entityTypeManager,
    private readonly AccountProxyInterface $account,
    private readonly RouteMatchInterface $routeMatch,
  ) {
    // Load the full user entity if the account is authenticated.
    $account_id = $this->account->id();
    if ($account_id && $account_id !== 0) {
      $user = $this->entityTypeManager->getStorage('user')->load($account_id);
      if ($user instanceof UserInterface) {
        $this->currentUser = $user;
      }
    }

    // Set source.
    $request = $this->requestStack->getCurrentRequest();
    $this->source = $request ? $request->getPathInfo() : '';

    // Set route name.
    $this->routeName = $this->routeMatch->getRouteName() ?: '';
  }

  /**
   * Create event handler.
   */
  public function eventCreate(NodeInterface $node): void {
    $event_type = 'com.getopensocial.cms.event.create';
    $topic_name = 'com.getopensocial.cms.event.create';
    $this->dispatch($topic_name, $event_type, $node);
  }

  /**
   * Delete event handler.
   */
  public function eventDelete(NodeInterface $node): void {
    $event_type = 'com.getopensocial.cms.event.delete';
    $topic_name = 'com.getopensocial.cms.event.delete';
    $this->dispatch($topic_name, $event_type, $node, 'delete');
  }

  /**
   * Publish event handler.
   */
  public function eventPublish(NodeInterface $node): void {
    $event_type = 'com.getopensocial.cms.event.publish';
    $topic_name = 'com.getopensocial.cms.event.publish';
    $this->dispatch($topic_name, $event_type, $node);
  }

  /**
   * Unpublish event handler.
   */
  public function eventUnpublish(NodeInterface $node): void {
    $event_type = 'com.getopensocial.cms.event.unpublish';
    $topic_name = 'com.getopensocial.cms.event.unpublish';
    $this->dispatch($topic_name, $event_type, $node);
  }

  /**
   * Update event handler.
   */
  public function eventUpdate(NodeInterface $node): void {
    $event_type = 'com.getopensocial.cms.event.update';
    $topic_name = 'com.getopensocial.cms.event.update';
    $this->dispatch($topic_name, $event_type, $node);
  }

  /**
   * Transforms a NodeInterface into a CloudEvent.
   *
   * @throws \Drupal\Core\Entity\EntityMalformedException
   * @throws \Drupal\Core\TypedData\Exception\MissingDataException
   */
  public function fromEntity(NodeInterface $node, string $event_type, string $op = ''): CloudEvent {
    // Determine actors.
    [$actor_application, $actor_user] = $this->determineActors();

    // List enrollment methods.
    $enrollment_methods = ['open', 'request', 'invite'];

    // Determine status.
    if ($op == 'delete') {
      $status = 'removed';
    }
    else {
      $status = $node->get('status')->value ? 'published' : 'unpublished';
    }

    return new CloudEvent(
      id: $this->uuid->generate(),
      source: $this->source,
      type: $event_type,
      data: [
        'event' => new EventEntityData(
          id: $node->get('uuid')->value,
          created: DateTime::fromTimestamp($node->getCreatedTime())->toString(),
          updated: DateTime::fromTimestamp($node->getChangedTime())->toString(),
          status: $status,
          label: (string) $node->label(),
          visibility: ContentVisibility::fromEntity($node),
          group: !$node->get('groups')->isEmpty() ? Entity::fromEntity($node->get('groups')->getEntity()) : NULL,
          author: User::fromEntity($node->get('uid')->entity),
          allDay: $node->get('field_event_all_day')->value,
          start: $node->get('field_event_date')->value,
          end: $node->get('field_event_date_end')->value,
          timezone: date_default_timezone_get(),
          address: Address::fromFieldItem(
            item: $node->get('field_event_address')->first(),
            label: $node->get('field_event_location')->value
          ),
          enrollment: [
            'enabled' => (bool) $node->get('field_event_enroll')->value,
            'method' => $enrollment_methods[$node->get('field_enroll_method')->value],
          ],
          href: Href::fromEntity($node),
          type: $node->hasField('field_event_type') && !$node->get('field_event_type')->isEmpty() ? $node->get('field_event_type')->getEntity()->label() : NULL,
        ),
        'actor' => [
          'application' => $actor_application ? Application::fromId($actor_application) : NULL,
          'user' => $actor_user ? User::fromEntity($actor_user) : NULL,
        ],
      ],
      dataContentType: 'application/json',
      dataSchema: NULL,
      subject: NULL,
      time: DateTime::fromTimestamp($node->getCreatedTime())->toImmutableDateTime(),
    );
  }

  /**
   * Determines the actor (application and user) for the CloudEvent.
   *
   * @return array
   *   An array with two elements: the application and the user.
   */
  private function determineActors(): array {
    $application = NULL;
    $user = NULL;

    switch ($this->routeName) {
      case 'entity.node.edit_form':
      case 'entity.node.delete_form':
      case 'entity.node.delete_multiple_form':
      case 'system.admin_content':
        $user = $this->currentUser;
        break;

      case 'entity.ultimate_cron_job.run':
        $application = 'cron';
        break;
    }

    return [
      $application,
      $user,
    ];
  }

  /**
   * Dispatches the event.
   *
   * @param string $topic_name
   *   The topic name.
   * @param string $event_type
   *   The event type.
   * @param \Drupal\node\NodeInterface $node
   *   The node object.
   * @param string $op
   *   The operation.
   *
   * @throws \Drupal\Core\Entity\EntityMalformedException
   * @throws \Drupal\Core\TypedData\Exception\MissingDataException
   */
  private function dispatch(string $topic_name, string $event_type, NodeInterface $node, string $op = ''): void {
    // Skip if required modules are not enabled.
    if (!$this->moduleHandler->moduleExists('social_eda')) {
      return;
    }

    // Build the event.
    $event = $this->fromEntity($node, $event_type, $op);

    // Dispatch to message broker.
    $this->dispatcher->dispatch($topic_name, $event);
  }

}
